/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdlib>
#include <cstdio>

// #include "logger.hh"
#include "commons.hh"
#include "hashtable.hh"

#define ROTATE_LEFT_32(x, r)        ((x << r) | (x >> (32 - r)))


struct List* soren::hash::LfHashTable::__bucketInit() {
    return reinterpret_cast<struct List*>(
            aligned_alloc(8, sizeof(struct List) * nelem));
}



void soren::hash::LfHashTable::__listInit(struct List* arg_list) {
    arg_list->head.next_slot = &arg_list->head;
}



/// @brief Inserts element between previous element and the next one.
/// @param arg_new 
/// @param arg_prev 
/// @param arg_next 
/// @return 
bool soren::hash::LfHashTable::__elemInsert(
    struct LocalSlot* arg_new, struct LocalSlot* arg_prev, struct LocalSlot* arg_next) {

    do {
        struct LocalSlot* orig_next;

        arg_new->next_slot = arg_next; 
        orig_next = CASV(&(arg_prev->next_slot), arg_next, arg_new);

        if (orig_next == arg_next) return true;
        if (orig_next != GET_MARKED_AS_PROTECTED(arg_next))
            return false;

        arg_next = orig_next;

    } while (1);
}



/// @brief Do switches of the elements.
/// @param arg_new 
/// @param arg_prev 
/// @param arg_next 
/// @return 
bool soren::hash::LfHashTable::__elemSwitch(
    struct LocalSlot* arg_new, struct LocalSlot* arg_prev, struct LocalSlot* arg_next) {

    do {
        struct LocalSlot* orig_next;

        arg_new->next_slot = arg_next;  

        orig_next = CASV(&(arg_prev->next_slot), 
            GET_MARKED_AS_PROTECTED(arg_next), 
            GET_MARKED_AS_DELETED(arg_new));

        if (orig_next == GET_MARKED_AS_PROTECTED(arg_next)) return true;
        if (!IS_MARKED_AS_PROTECTED(orig_next)) return false;

        arg_next = GET_UNMARKED_REFERENCE(orig_next); 

    } while (1);
}



/// @brief Deletes an element.
/// @param arg_target 
void soren::hash::LfHashTable::__elemDelete(struct LocalSlot* arg_target) {

    struct LocalSlot* next = arg_target->next_slot;

    do {
        struct LocalSlot* orig_next;

        if (IS_MARKED_REFERENCE(next, ALL)) return; 

        orig_next = CASV(&(arg_target->next_slot), next, GET_MARKED_AS_DELETED(next));
        
        if (IS_SAME_BASEREF(orig_next, next))
            return;

        next = orig_next;

    } while (1);   
}



/// @brief Do cleanups for a list, managed by the hash table instance.
/// @param arg_list 
void soren::hash::LfHashTable::__elemCleanups(struct List* arg_list) {

    struct LocalSlot *curr, *prev;           // current cell & previous cell.  
    struct LocalSlot *curr_next, *prev_next; // 

    prev = &arg_list->head;
    curr = prev->next_slot; 
    curr_next = curr->next_slot;

    while (curr != &arg_list->head) {

        if (!IS_MARKED_AS_DELETED(curr_next)) {
            if (!IS_SAME_BASEREF(prev_next, curr))
                CAS(&(prev->next_slot), prev_next, GET_SAME_MARK(curr, prev_next));

            prev = curr; 
            prev_next = prev->next_slot; 
        }

        curr = GET_UNMARKED_REFERENCE(curr_next);
        curr_next = curr->next_slot;
    }

    if (!IS_SAME_BASEREF(prev_next, curr))
        CAS(&(prev->next_slot), prev_next, GET_SAME_MARK(curr, prev_next));
}



struct List* soren::hash::LfHashTable::__getBucket(uint32_t arg_idx) {
    
    if (arg_idx >= nelem) arg_idx %= nelem;
    return &bucket[arg_idx];
}



bool soren::hash::LfHashTable::__elemSearch(
    uint32_t arg_hashval, struct LocalSlot* arg_key, struct LocalSlot** arg_left, struct LocalSlot** arg_right) {
        
    struct List* list = __getBucket(arg_hashval);

    struct LocalSlot* prev       = &list->head;
    struct LocalSlot* prev_next  = prev->next_slot;
    struct LocalSlot* curr       = prev->next_slot; 
    struct LocalSlot* curr_next  = curr->next_slot;

    while (curr != &list->head && compare_func(curr, arg_key) <= 0) {
        
        if (!IS_MARKED_AS_DELETED(curr_next)) {
            if (!IS_SAME_BASEREF(prev_next, curr)) {
                CAS(&(prev->next_slot), prev_next, GET_SAME_MARK(curr, prev_next));
            }

            if (compare_func(arg_key, curr) == 0) {                
                do {
                    struct LocalSlot* orig_curr_next; 

                    orig_curr_next = CASV(&(curr->next_slot), curr_next, 
                        GET_MARKED_AS_PROTECTED(curr_next));

                    if (orig_curr_next == curr_next || 
                        orig_curr_next == GET_MARKED_AS_PROTECTED(curr_next)) 
                    {
                        *arg_left  = curr;
                        *arg_right = GET_UNMARKED_REFERENCE(curr_next); 
                        return true; 
                    }

                    curr_next = orig_curr_next;

                } while (!IS_MARKED_AS_DELETED(curr_next));
            }

            prev = curr; 
            prev_next = prev->next_slot; 
        }

        curr = GET_UNMARKED_REFERENCE(curr_next);
        curr_next = curr->next_slot;
    }

    if (!IS_SAME_BASEREF(prev_next, curr)) {
        CAS(&(prev->next_slot), prev_next, GET_SAME_MARK(curr, prev_next));
    }

    *arg_left  = prev; 
    *arg_right = curr; 
    
    return false;
}



/// @brief MurmurHash3 : Generates hash value.
/// @param arg_key 
/// @param arg_len 
/// @param arg_seed 
/// @return h1
uint32_t soren::hash::LfHashTable::__murmurHash3(const void* arg_key, int arg_len, uint32_t arg_seed) {

    const uint8_t* data = reinterpret_cast<const uint8_t*>(arg_key);
    const int nblocks = arg_len / 4;

    uint32_t h1 = arg_seed;

    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;

    uint32_t k1;

    const uint32_t* blocks = reinterpret_cast<const uint32_t*>(data + nblocks * 4);
    
    for (int i = -nblocks; i; i++) {
        k1 = blocks[i];
        k1 *= c1; 

        k1 = ROTATE_LEFT_32(k1, 15); 
        k1 *= c2;

        h1 ^= k1; 
        h1 = ROTATE_LEFT_32(h1, 13); 
        h1 = h1 * 5 + 0xe6546b64;
    }

    // For last 1 ~ 3 byte ==> Tail
    const uint8_t *tail = reinterpret_cast<const uint8_t*>(data + nblocks * 4);

    k1 = 0;

    switch (arg_len & 3) {
        case 3:     k1 ^= tail[2] << 16;
        case 2:     k1 ^= tail[1] << 8;
        case 1:     k1 ^= tail[0];
        
        k1 *= c1; 
        k1 = ROTATE_LEFT_32(k1, 15); 
        k1 *= c2; 
        h1 ^= k1;
    }

    h1 ^= arg_len; 

    h1 ^= (h1 >> 16);
    h1 *= 0x85ebca6b;
    h1 ^= (h1 >> 13);
    h1 *= 0xc2b2ae35;
    h1 ^= (h1 >> 16);

    return h1;
}



soren::hash::LfHashTable::LfHashTable(uint32_t arg_nelem, compfunc_t arg_compfunc) 
    : nelem(arg_nelem), compare_func(arg_compfunc) {

    // Allocate buckets
    if ((bucket = __bucketInit()) == nullptr) abort();

    // Init lists
    for (int idx = 0; idx < nelem; idx++)
        __listInit(&bucket[idx]);
}



soren::hash::LfHashTable::~LfHashTable() {
    free(bucket);
}



uint32_t soren::hash::LfHashTable::doHash(const void* arg_key, int arg_len) {
    return __murmurHash3(arg_key, arg_len, 0);
}



bool soren::hash::LfHashTable::doInsert(struct LocalSlot* arg_new, struct LocalSlot* arg_prev, struct LocalSlot* arg_next) {
    return __elemInsert(arg_new, arg_prev, arg_next);
}



bool soren::hash::LfHashTable::doSwitch(struct LocalSlot* arg_new, struct LocalSlot* arg_prev, struct LocalSlot* arg_next) {
    return __elemSwitch(arg_new, arg_prev, arg_next);
}



bool soren::hash::LfHashTable::doSearch(
    uint32_t arg_hashval, struct LocalSlot* arg_key, struct LocalSlot** arg_left, struct LocalSlot** arg_right) {
    
    return __elemSearch(arg_hashval, arg_key, arg_left, arg_right);
}



struct List* soren::hash::LfHashTable::debugGetBucket(uint32_t arg_idx) {
    return __getBucket(arg_idx);
}