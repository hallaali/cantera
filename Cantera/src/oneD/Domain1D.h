/**
 *  @file Domain1D.h
 *
 *  $Author$
 *  $Date$
 *  $Revision$
 *
 *  Copyright 2002 California Institute of Technology
 *
 */

#ifndef CT_DOMAIN1D_H
#define CT_DOMAIN1D_H


#include "../ctexceptions.h"
#include "../xml.h"
#include "refine.h"


namespace Cantera {

    // domain types
    const int cFlowType         = 50;
    const int cConnectorType    = 100;
    const int cSurfType         = 102;
    const int cInletType        = 104;
    const int cSymmType         = 105;
    const int cOutletType       = 106;

    class MultiJac;
    class OneDim;


    /**
     * Base class for one-dimensional domains.
     */
    class Domain1D {
    public:

        /**
         * Constructor.
         * @param nv      Number of variables at each grid point.
         * @param points  Number of grid points.
         */
        Domain1D(int nv=1, int points=1, 
            doublereal time = 0.0) : 
            m_time(time),
            m_container(0), 
            m_index(-1),
            m_type(0),
            m_iloc(0),
            m_jstart(0),
            m_left(0), 
            m_right(0),
            m_refiner(0) {
            resize(nv, points);
        }

        /// Destructor. Does nothing
        virtual ~Domain1D(){ delete m_refiner; }

        /// Domain type flag.
        const int domainType() { return m_type; }

        /**
         * The left-to-right location of this domain.
         */
        const int domainIndex() { return m_index; }

        bool isConnector() { return (m_type >= cConnectorType); }

        /**
         * The container holding this domain.
         */
        const OneDim& container() const { return *m_container; }

        /**
         * Specify the container object for this domain, and the
         * position of this domain in the list.
         */
        void setContainer(OneDim* c, int index){
            m_container = c;
            m_index = index;
        }

        /** 
         * Initialize. Base class method does nothing, but may be
         * overloaded. 
         */
        virtual void init(){}

        virtual void setInitialState(doublereal* xlocal = 0){}
        virtual void setState(int point, const doublereal* state, doublereal* x) {}

        /**
         * Resize the domain to have nv components and np grid points.
         * This method is virtual so that subclasses can perform other
         * actions required to resize the domain.
         */
        virtual void resize(int nv, int np) {
            if (nv != m_nv || !m_refiner) {
                m_nv = nv;
                delete m_refiner;
                m_refiner = new Refiner(*this);
            }
            m_nv = nv;
            m_max.resize(m_nv, 0.0);
            m_min.resize(m_nv, 0.0);
            m_rtol_ss.resize(m_nv, 1.0e-8);
            m_atol_ss.resize(m_nv, 1.0e-15);
            m_rtol_ts.resize(m_nv, 1.0e-8);
            m_atol_ts.resize(m_nv, 1.0e-15);
            m_points = np;
            m_z.resize(np, 0.0);
            m_slast.resize(m_nv * m_points, 0.0);
            locate();
        }

        Refiner& refiner() { return *m_refiner; }

        /// Number of components at each grid point.
        int nComponents() const { return m_nv; }

        /// Number of grid points in this domain.
        int nPoints() const { return m_points; }

        /// Name of the nth component. May be overloaded.
        virtual string componentName(int n) const { 
            return "component " + int2str(n); 
        }

        int componentIndex(string name) {
            int nc = nComponents();
            for (int n = 0; n < nc; n++) {
                if (name == componentName(n)) return n;
            }
            throw CanteraError("Domain1D::componentIndex",
                "no component named "+name);
        }

        /**
         * Set the lower and upper bounds for each solution component.
         */
        void setBounds(int nl, const doublereal* lower, 
            int nu, const doublereal* upper) {
            if (nl < m_nv || nu < m_nv)
                throw CanteraError("Domain1D::setBounds",
                    "wrong array size for solution bounds. "
                    "Size should be at least "+int2str(m_nv));
            copy(upper, upper + m_nv, m_max.begin());
            copy(lower, lower + m_nv, m_min.begin());
        }

        void setBounds(int n, doublereal lower, doublereal upper) {
            m_min[n] = lower;
            m_max[n] = upper;
        }

        void setTolerances(int nr, const doublereal* rtol, 
            int na, const doublereal* atol, int ts = 0) {
            if (nr < m_nv || na < m_nv)
                throw CanteraError("Domain1D::setTolerances",
                    "wrong array size for solution error tolerances. "
                    "Size should be at least "+int2str(m_nv));
            if (ts >= 0) {
                copy(rtol, rtol + m_nv, m_rtol_ss.begin());
                copy(atol, atol + m_nv, m_atol_ss.begin());
            }
            if (ts <= 0) {
                copy(rtol, rtol + m_nv, m_rtol_ts.begin());
                copy(atol, atol + m_nv, m_atol_ts.begin());
            }
        }

        void setTolerances(int n, doublereal rtol, doublereal atol, int ts = 0) {
            if (ts >= 0) {
                m_rtol_ss[n] = rtol;
                m_atol_ss[n] = atol;
            }
            if (ts <= 0) {
                m_rtol_ts[n] = rtol;
                m_atol_ts[n] = atol;
            }
        }

        /// Relative tolerance of the nth component.
        doublereal rtol(int n) { return (m_rdt == 0.0 ? m_rtol_ss[n] : m_rtol_ts[n]); }

        /// Absolute tolerance of the nth component.
        doublereal atol(int n) { return (m_rdt == 0.0 ? m_atol_ss[n] : m_atol_ts[n]); }

        /// Upper bound on the nth component.
        doublereal upperBound(int n) const { return m_max[n]; }

        /// Lower bound on the nth component
        doublereal lowerBound(int n) const { return m_min[n]; }


        /**
         * Prepare to do time stepping with time step dt. Copy the
         * internally-stored solution at the last time step to array
         * x0.
         */
        void initTimeInteg(doublereal dt, const doublereal* x0) {
            copy(x0 + loc(), x0 + loc() + size(), m_slast.begin());
            m_rdt = 1.0/dt;
        } 
        
        /**
         * Prepare to solve the steady-state problem.
         * Set the internally-stored reciprocal of the time step to 0,0
         */
        void setSteadyMode() { m_rdt = 0.0; }

        /// True if in steady-state mode
        bool steady() { return (m_rdt == 0.0); }

        /// True if not in steady-state mode
        bool transient() { return (m_rdt != 0.0); }

        /**
         * Set this if something has changed in the governing
         * equations (e.g. the value of a constant has been changed,
         * so that the last-computed Jacobian is no longer valid.
         * Note: see file OneDim.cpp for the implementation of this method.
         */
        void needJacUpdate();

        /**
         * Evaluate the steady-state residual at all points, even if in 
         * transient mode. Used only to print diagnostic output.
         */
        void evalss(doublereal* x, doublereal* r, integer* mask) {
            eval(-1,x,r,mask,0.0);
        }

        /**
         * Evaluate the residual function at point j. If j < 0,
         * evaluate the residual function at all points.
         */         
        virtual void eval(int j, doublereal* x, doublereal* r, 
            integer* mask, doublereal rdt=0.0) {
            throw CanteraError("Domain1D::eval",
                "residual function not defined.");
        }

        /**
         * Does nothing.
         */
        virtual void update(doublereal* x) {}

        doublereal time() const { return m_time;}
        void incrementTime(doublereal dt) { m_time += dt; }
        size_t index(int n, int j) const { return m_nv*j + n; }
        doublereal value(doublereal* x, int n, int j) const {
            return x[index(n,j)];
        }

        virtual void setJac(MultiJac* jac){}
        virtual void save(XML_Node& o, doublereal* sol) {
            throw CanteraError("Domain1D::save","base class method called");
        }

        int size() const { return m_nv*m_points; }

        /**
         * Find the index of the first grid point in this domain, and
         * the start of its variables in the global solution vector.
         */
        void locate() {

            if (m_left) {
                // there is a domain on the left, so the first grid point
                // in this domain is one more than the last one on the left
                m_jstart = m_left->lastPoint() + 1;

                // the starting location in the solution vector
                m_iloc = m_left->loc() + m_left->size();
            }
            else {
                // this is the left-most domain
                m_jstart = 0;
                m_iloc = 0;
            }
            // if there is a domain to the right of this one, then 
            // repeat this for it
            if (m_right) m_right->locate();
        }

        /**
         * Location of the start of the local solution vector in the global
         * solution vector,
         */
        virtual int loc(int j = 0) const { return m_iloc; }

        /**
         * The index of the first (i.e., left-most) grid point
         * belonging to this domain.
         */
        int firstPoint() const { return m_jstart; }

        /**
         * The index of the last (i.e., right-most) grid point
         * belonging to this domain.
         */
        int lastPoint() const { return m_jstart + m_points - 1; }

        /**
         * Set the left neighbor to domain 'left.' Method 'locate' is
         * called to update the global positions of this domain and
         * all those to its right.
         */
        void linkLeft(Domain1D* left) { 
            m_left = left;
            locate();
        }

        /**
         * Set the right neighbor to domain 'right.' 
         */
        void linkRight(Domain1D* right) { m_right = right; }

        /**
         * Append domain 'right' to this one, and update all links.
         */
        void append(Domain1D* right) {
            linkRight(right);
            right->linkLeft(this);
        }

        /**
         * Return a pointer to the left neighbor.
         */
        Domain1D* left() const { return m_left; }

        /**
         * Return a pointer to the right neighbor.
         */
        Domain1D* right() const { return m_right; }

        /**
         * Value of component n at point j in the previous solution.
         */
        double prevSoln(int n, int j) const {
            return m_slast[m_nv*j + n];
        }
        
        /**
         * Specify an identifying tag for this domain.
         */
        void setID(const string& s) {m_id = s;}

        string id() { 
            if (m_id != "") return m_id;
            else return string("domain ") + int2str(m_index);
        }

        /**
         * Specify descriptive text for this domain.
         */
        void setDesc(const string& s) {m_desc = s;}
        const string& desc() { return m_desc; }

        virtual void getTransientMask(integer* mask){}

        virtual void showSolution(ostream& s, const doublereal* x) {}
        virtual void showSolution(const doublereal* x) {}

        virtual void restore(const XML_Node& dom, doublereal* soln) {}

        doublereal z(int jlocal) const {
            return m_z[jlocal];
        }
        doublereal zmin() const { return m_z[0]; }
        doublereal zmax() const { return m_z[m_points - 1]; }


        void setProfile(string name, doublereal* values, doublereal* soln) {
            int n, j;
            for (n = 0; n < m_nv; n++) {
                if (name == componentName(n)) {
                    for (j = 0; j < m_points; j++) {
                        soln[index(n, j) + m_iloc] = values[j];
                    }
                    return;
                }
            }
            throw CanteraError("Domain1D::setProfile",
                "unknown component: "+name);
        }

        vector_fp& grid() { return m_z; }
        const vector_fp& grid() const { return m_z; }
        doublereal grid(int point) { return m_z[point]; }

        virtual void setupGrid(int n, const doublereal* z) {}

        /**
         * Writes some or all initial solution values into array x,
         * which is the solution vector for this domain. This allows
         * initial values that have been set prior to installing this
         * domain into the container to be written to the global
         * solution vector.
         */
        virtual void _getInitialSoln(doublereal* x) {
            throw CanteraError("Domain1D::_getInitialSoln",
                "base class method _getInitialSoln called!");
        }

        /**
         * Perform any necessary domain-specific initialization using 
         * local solution vector x.
         */ 
        virtual void _finalize(const doublereal* x) {
            throw CanteraError("Domain1D::_finalize",
                "base class method _finalize called!");
        }


    protected:

        doublereal m_rdt;
        int m_nv;
        int m_points;
        vector_fp m_slast;
        doublereal m_time;
        vector_fp m_max;
        vector_fp m_min;
        vector_fp m_rtol_ss, m_rtol_ts;
        vector_fp m_atol_ss, m_atol_ts;
        vector_fp m_z;
        OneDim* m_container;
        int m_index;
        int m_type;
        int m_iloc;
        int m_jstart;
        Domain1D *m_left, *m_right;
        string m_id, m_desc;
        Refiner* m_refiner;

    private:

    };
}

#endif


