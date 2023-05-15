#pragma once
/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <atomic>
#include <array>


#include "hashtable.hh"

namespace soren {

    enum {
        RETCODE_ERROR       = -1,
        RETCODE_SWITCHED    =  0,
        RETCODE_INSERTED
    };

    const int HASHTABLE_SZ          = (1 << 20);
    const int HASHTABLE_NBUCKETS    = (HASHTABLE_SZ >> 3);
    // const int HASHTABLE_NBUCKETS    = 50;

    class DependencyChecker {
    private:
        hash::LfHashTable hash_table;
        std::array<std::atomic<bool>, HASHTABLE_NBUCKETS> is_deleting;

    public:
        DependencyChecker(uint32_t = HASHTABLE_NBUCKETS, compfunc_t = localSlotHashComp);
        ~DependencyChecker() {};

        // Interface 
        uint32_t doHash(const void*, int);
        void doTryInsert(LocalSlot*, const void*, int);
        void doTryInsert(LocalSlot*);
        void doDelete(LocalSlot*);

        bool doSearch(uint32_t, struct LocalSlot*, struct LocalSlot**, struct LocalSlot**);
        struct LocalSlot* getNextValidSlot(struct LocalSlot*);

        void doResetAll();

        // For debug
        void printBucket(struct List*);
        void printAll();
    };
}