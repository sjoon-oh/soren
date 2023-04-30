/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "commons.hh"
#include "timer.hh"

#include "dependency.hh"
#include <iostream>


soren::DependencyChecker::DependencyChecker(uint32_t arg_nelem, compfunc_t arg_compfunc) 
    : hash_table(arg_nelem, arg_compfunc) { }



int soren::DependencyChecker::doTryInsert(LocalSlot* arg_slot, const void* arg_buf, int arg_len) {

    bool is_samekey, is_success;
    LocalSlot *prev, *next;

    arg_slot->hashed_key = hash_table.doHash(arg_buf, arg_len);
    GET_TIMESTAMP((arg_slot->timestamp));

    is_samekey = hash_table.doSearch(arg_slot->hashed_key, arg_slot, &prev, &next);

    if (!is_samekey) {
        is_success = hash_table.doInsert(arg_slot, prev, next);
        
        if (!is_success) return RETCODE_ERROR;
        else {
            arg_slot->footprint = FOOTPRINT_INSERTED;
            return RETCODE_INSERTED;
        }
    }
    else {
        is_success = hash_table.doSwitch(arg_slot, prev, next);

        if (!is_success) return RETCODE_ERROR;
        else { 
            arg_slot->footprint = FOOTPRINT_SWITCHED;
            return RETCODE_SWITCHED;
        }
    }
}



void soren::DependencyChecker::doDelete(LocalSlot* arg_slot) {
    hash_table.doDelete(arg_slot);
}

