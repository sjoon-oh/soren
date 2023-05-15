/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "commons.hh"
#include "timer.hh"

#include "logger.hh"

#include "dependency.hh"

// #include <algorithm>
#include <iostream>


//
// Replicator methods
namespace soren {
    static LoggerFileOnly DEPCHECKER_LGR("DepChecker", "soren_depchecker.log");
}


soren::DependencyChecker::DependencyChecker(uint32_t arg_nelem, compfunc_t arg_compfunc) 
    : hash_table(arg_nelem, arg_compfunc) { 

        for (auto& clean_flag: is_deleting) clean_flag.store(false);
    }



uint32_t soren::DependencyChecker::doHash(const void* arg_buf, int arg_len) {
    return hash_table.doHash(arg_buf, arg_len);
}



void soren::DependencyChecker::doTryInsert(LocalSlot* arg_slot, const void* arg_buf, int arg_len) {

    bool is_samekey, is_success;
    LocalSlot *prev, *next;

    arg_slot->hashed_key = hash_table.doHash(arg_buf, arg_len);
    GET_TIMESTAMP((arg_slot->timestamp));

    while(is_deleting.at(arg_slot->hashed_key % HASHTABLE_NBUCKETS).load() == true)
        ;

    do {
        is_samekey = hash_table.doSearch(arg_slot->hashed_key, arg_slot, &prev, &next);

        if (!is_samekey)
            is_success = hash_table.doInsert(arg_slot, prev, next);
        else
            is_success = hash_table.doSwitch(arg_slot, prev, next);

    } while(!is_success);

    // Debug
    // printBucket(hash_table.getBucket(arg_slot->hashed_key));
}



void soren::DependencyChecker::doTryInsert(LocalSlot* arg_slot) {

    bool is_samekey, is_success;
    LocalSlot *prev, *next;

    GET_TIMESTAMP((arg_slot->timestamp));

    while(is_deleting.at(arg_slot->hashed_key % HASHTABLE_NBUCKETS).load() == true)
        ;

    do {
        is_samekey = hash_table.doSearch(arg_slot->hashed_key, arg_slot, &prev, &next);

        if (!is_samekey)
            is_success = hash_table.doInsert(arg_slot, prev, next);
        else
            is_success = hash_table.doSwitch(arg_slot, prev, next);

    } while(!is_success);

    // Debug
    // printBucket(hash_table.getBucket(arg_slot->hashed_key));
}



void soren::DependencyChecker::doDelete(LocalSlot* arg_slot) {
    hash_table.doDelete(arg_slot);
}



bool soren::DependencyChecker::doSearch(
    uint32_t arg_hashval, struct LocalSlot* arg_key, struct LocalSlot** arg_left, struct LocalSlot** arg_right) {

    while(is_deleting.at(arg_hashval % HASHTABLE_NBUCKETS).load() == true)
        ;
    
    return hash_table.doSearch(arg_hashval, arg_key, arg_left, arg_right);
}



struct soren::LocalSlot* soren::DependencyChecker::getNextValidSlot(struct LocalSlot* arg_front) {

    struct LocalSlot* current_slot = arg_front;
    struct LocalSlot* next_slot;
    
    uint32_t target_hashval = arg_front->hashed_key;
    struct List* bucket = hash_table.getBucket(target_hashval);

    //
    // Deleted element appends the new valid ones at the end.
    while (current_slot != &bucket->head) {

        next_slot = current_slot->next_slot;
        
        if (IS_MARKED_AS_DELETED(next_slot))
            current_slot = GET_UNMARKED_REFERENCE(next_slot);

        else {
            if (current_slot->hashed_key == target_hashval)
                return current_slot;
            else return nullptr;
        }
    }
}



void soren::DependencyChecker::doResetAll() {
    hash_table.doResetAll();
}



void soren::DependencyChecker::printBucket(struct List* arg_list) {
    
    struct soren::LocalSlot* current_slot       = GET_UNMARKED_REFERENCE(arg_list->head.next_slot);
    struct soren::LocalSlot* next_slot;

    uint32_t hashed_key, front_val = 0;
    uint16_t owner, footprint;

    uint32_t size = 1;

    SOREN_LOGGER_INFO(DEPCHECKER_LGR, "-------- List Print Start --------");
    SOREN_LOGGER_INFO(DEPCHECKER_LGR, "          > ~~~ HEAD ~~~");
    
    //
    // The current slot's status is marked at next_slot member.
    while (current_slot != &arg_list->head) {
        
        hashed_key  = current_slot->hashed_key;
        owner       = current_slot->header.owner;
        footprint   = current_slot->footprint;

        next_slot = current_slot->next_slot;

        if (IS_MARKED_AS_DELETED(next_slot))
            SOREN_LOGGER_INFO(DEPCHECKER_LGR, "[Deleted] > [{:3d}] Hash: {:12d}, Owned: {:4d}, OW({})", size, hashed_key, owner, footprint);
        
        else if (IS_MARKED_AS_PROTECTED(next_slot))
            SOREN_LOGGER_INFO(DEPCHECKER_LGR, "[Protect] > [{:3d}] Hash: {:12d}, Owned: {:4d}, OW({})", size, hashed_key, owner, footprint);

        else 
            SOREN_LOGGER_INFO(DEPCHECKER_LGR, "[       ] > [{:3d}] Hash: {:12d}, Owned: {:4d}, OW({})", size, hashed_key, owner, footprint);

        size++;

        current_slot        = GET_UNMARKED_REFERENCE(next_slot);
    }

    SOREN_LOGGER_INFO(DEPCHECKER_LGR, "          > ~~~ TAIL ~~~");
    SOREN_LOGGER_INFO(DEPCHECKER_LGR, "-------- List Print End --------\n");
}



void soren::DependencyChecker::printAll() {
    for (int bucket_idx = 0; bucket_idx < is_deleting.size(); bucket_idx++) {
        printBucket(hash_table.getBucketByIdx(bucket_idx));
    }
}