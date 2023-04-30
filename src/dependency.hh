#pragma once
/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "hashtable.hh"

namespace soren {

    enum {
        RETCODE_ERROR       = -1,
        RETCODE_SWITCHED    =  0,
        RETCODE_INSERTED
    };

    const int HASHTABLE_SZ          = (1 << 20);
    const int HASHTABLE_NBUCKETS    = (HASHTABLE_SZ >> 3);

    class DependencyChecker {
    private:
        hash::LfHashTable hash_table;

    public:
        DependencyChecker(uint32_t = HASHTABLE_NBUCKETS, compfunc_t = localSlotHashComp);
        ~DependencyChecker() {};

        // Interface 
        int doTryInsert(LocalSlot*, const void*, int);
        void doDelete(LocalSlot*);
    };
}