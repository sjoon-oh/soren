/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "commons.hh"
#include "timer.hh"

const bool soren::isSlotValid(HeaderSlot& arg_tar, uint32_t arg_can) { return arg_tar.canary == arg_can; }



/// @brief Compares the timestamp of a LocalSlot instance.
/// @param arg_tar_a 
/// @param arg_tar_b 
/// @return 
int soren::localSlotTsComp(void* arg_tar_a, void* arg_tar_b) {

    struct timespec ts_a = reinterpret_cast<LocalSlot*>(arg_tar_a)->timestamp;
    struct timespec ts_b = reinterpret_cast<LocalSlot*>(arg_tar_b)->timestamp;

    if (ts_a.tv_sec == ts_b.tv_sec) return ts_a.tv_nsec < ts_b.tv_nsec;
    else return ts_a.tv_sec < ts_b.tv_sec;
}



/// @brief Compares the hashed value of a LocalSlot instance.
/// @param arg_tar_a 
/// @param arg_tar_a 
/// @param arg_tar_b 
/// @return 
int soren::localSlotHashComp(void* arg_tar_a, void* arg_tar_b) {

    LocalSlot* target_a = reinterpret_cast<LocalSlot*>(arg_tar_a);
    LocalSlot* target_b = reinterpret_cast<LocalSlot*>(arg_tar_b);

    if (target_a->hashed_key == target_b->hashed_key) 
        return 0;

    else if (target_a->hashed_key > target_b->hashed_key)
        return 1;

    else return -1;
}