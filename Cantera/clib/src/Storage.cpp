
// Build as a DLL under Windows
#ifdef WIN32
#pragma warning(disable:4786)
#pragma warning(disable:4503)
#else
#define DLL_EXPORT
#endif


// Cantera includes
#include "Kinetics.h"
#include "transport/TransportFactory.h"

#include "clib_defs.h"
#include "Storage.h"


Storage::Storage() {
    addThermo(new ThermoPhase);
    addKinetics(new Kinetics);
    addTransport(newTransportMgr());
}

Storage::~Storage() { clear(); }

int Storage::addThermo(thermo_t* th) {
    if (th->index() >= 0)
        return th->index();
    __thtable.push_back(th);
    int n = __thtable.size() - 1;
    th->setIndex(n);
    __thmap[th->id()] = n;
    return n;
}

int Storage::nThermo() { return __thtable.size(); }

int Storage::addKinetics(Kinetics* kin) {
    if (kin->index() >= 0)
        return kin->index();
    __ktable.push_back(kin);
    int n = __ktable.size() - 1;
    kin->setIndex(n);
    return n;
}

int Storage::addTransport(Transport* tr) {
    if (tr->index() >= 0)
        return tr->index();
    __trtable.push_back(tr);
    int n = __trtable.size() - 1;
    tr->setIndex(n);
    return n;
}

// int Storage::addNewTransport(int model, char* dbase, int th, 
//     int loglevel) {
//     try {
//         ThermoPhase* thrm = __thtable[th];
//         Transport* tr = newTransportMgr(model, 
//             string(dbase), thrm, loglevel);
//         __trtable.push_back(tr);
//         return __trtable.size() - 1;
//     }
//     catch (CanteraError) {return -1;}
//     catch (...) {return ERR;}
// }

int Storage::clear() {
    int i, n;
    n = __thtable.size();
    for (i = 1; i < n; i++) {
        if (__thtable[i] != __thtable[0]) {
            delete __thtable[i];
            __thtable[i] = __thtable[0];
        }
    }
    n = __ktable.size();
    for (i = 1; i < n; i++) {
        if (__ktable[i] != __ktable[0]) {
            delete __ktable[i];
            __ktable[i] = __ktable[0];
        }
    }
    n = __trtable.size();
    for (i = 1; i < n; i++) {
        if (__trtable[i] != __trtable[0]) {
            delete __trtable[i];
            __trtable[i] = __trtable[0];
        }
    }
    return 0;
}

void Storage::deleteKinetics(int n) {
    if (n == 0) return;
    if (__ktable[n] != __ktable[0])
        delete __ktable[n];
    __ktable[n] = __ktable[0];
}

void Storage::deleteThermo(int n) {
    if (n == 0) return;
    if (n < 0 || n >= (int) __thtable.size())
        throw CanteraError("deleteThermo","illegal index");
    __thtable[n] = __thtable[0];
}

void Storage::deleteTransport(int n) {
    if (n == 0) return;
    if (__trtable[n] != __trtable[0])
        delete __trtable[n];
    __trtable[n] = __trtable[0];
}

Storage* Storage::__storage = 0;

