#pragma once
/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

//
// This codes are originated from Dohyun Kim (ehgus421210@kaist.ac.kr),
//  slightly refactored.

#include <cstdint>

#include "commons.hh"

//
// Basic building blocks.
#define CAS(p, o, n)    __sync_bool_compare_and_swap((uint64_t *) p, o, n)
#define CASV(p, o, n)   ((typeof(*p)) __sync_val_compare_and_swap((uint64_t *)p, o, n))
#define FAA(p, v)       ((typeof(p)) __sync_fetch_and_add((uint64_t *) &p, v))

#define XXX             0x1
#define YYY             0x2
#define ALL             (XXX | YYY)

#define IS_MARKED_REFERENCE(c, m)       ((((uint64_t) (c)) & (m)) != 0)
#define IS_MARKED_AS_DELETED(c)         IS_MARKED_REFERENCE(c, XXX)
#define IS_MARKED_AS_PROTECTED(c)       IS_MARKED_REFERENCE(c, YYY)

#define GET_UNMARKED_REFERENCE(c)       (typeof(c))(((uint64_t) (c)) & ~ALL)
#define GET_MARKED_REFERENCE(c, m)      (typeof(c))((((uint64_t) (c)) & ~ALL) | (m))
#define GET_MARKED_AS_DELETED(c)        GET_MARKED_REFERENCE(c, XXX)
#define GET_MARKED_AS_PROTECTED(c)      GET_MARKED_REFERENCE(c, YYY)

#define IS_SAME_BASEREF(a, b)           (GET_UNMARKED_REFERENCE(a) == GET_UNMARKED_REFERENCE(b))
#define GET_SAME_MARK(a, b)             (typeof(a))(((uint64_t) (a)) | (((uint64_t) (b)) & ALL))


typedef int32_t (*compfunc_t) (void *, void *);

struct __attribute__((packed)) Cell {
    struct soren::LocalSlot* next; 
};

struct __attribute__((packed)) List {
    struct soren::LocalSlot head;
};


//
// Wrapping start.
namespace soren {
    namespace hash {

        class LfHashTable {
        private:
            uint32_t        nelem;
            struct List*    bucket;

            compfunc_t      compare_func;

            //
            // Initialization functions
            struct List* __bucketInit();        // Initializes buckets
            void __listInit(struct List*);      // Initializes the list

            // Modification Interfaces
            bool __elemInsert(struct LocalSlot*, struct LocalSlot*, struct LocalSlot*);
            bool __elemSwitch(struct LocalSlot*, struct LocalSlot*, struct LocalSlot*);
            void __elemDelete(struct LocalSlot*);
            void __elemCleanups(struct List*);
            void __elemCleanupAfterSlot(struct List*, struct LocalSlot*);

            struct List* __getBucket(uint32_t);
            bool __elemSearch(uint32_t, struct LocalSlot*, struct LocalSlot**, struct LocalSlot**);


            // Core hash.
            uint32_t __murmurHash3(const void*, int, uint32_t);
            
        public:

            LfHashTable(uint32_t, compfunc_t);
            ~LfHashTable();
            
            uint32_t doHash(const void*, int);

            // Interfaces.
            bool doInsert(struct LocalSlot*, struct LocalSlot*, struct LocalSlot*);
            bool doSwitch(struct LocalSlot*, struct LocalSlot*, struct LocalSlot*);
            bool doSearch(uint32_t, struct LocalSlot*, struct LocalSlot**, struct LocalSlot**);
            void doDelete(struct LocalSlot*);
            void doCleanups(struct List*);
            void doCleanupAfterSlot(struct List*, struct LocalSlot*);

            void doResetAll();

            struct List* getBucket(uint32_t);
            struct List* getBucketByIdx(uint32_t);
        };

    }
}
