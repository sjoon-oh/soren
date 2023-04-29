/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>
#include <random>

#include <thread>
#include <functional>

#include "../soren.hh"

static soren::LoggerFileOnly soren_lgr_ins_swt("hash-ins-swt", "soren-hashtable-mt-ins-swt.test.log");
static soren::LoggerFileOnly soren_lgr_switch("hash-switch", "soren-hashtable-mt-switch.test.log");
static soren::LoggerFileOnly soren_lgr_delete("hash-delete", "soren-hashtable-mt-delete.test.log");

//
// Random generator.
void generateRandomStr(std::mt19937& arg_gen, char* arg_buf, int arg_len) {

    std::uniform_int_distribution<int> distributor(33, 126);        // Printable ASCII range.

    for (int i = 0; i < arg_len; i++)
        arg_buf[i] = static_cast<char>(distributor(arg_gen));
}

//
// Prints a single bucket.
void printAll(soren::LoggerFileOnly* arg_logger, struct List* arg_list) {
    
    struct soren::LocalSlot* current_slot       = GET_UNMARKED_REFERENCE(arg_list->head.next_slot);
    struct soren::LocalSlot* next_slot;

    uint32_t hashed_key, front_val = 0;
    uint16_t owner;

    uint32_t size = 1;

    SOREN_LOGGER_INFO(*arg_logger, "-------- List Print Start --------");
    SOREN_LOGGER_INFO(*arg_logger, "          > ~~~ HEAD ~~~");
    
    //
    // The current slot's status is marked at next_slot member.
    while (current_slot != &arg_list->head) {
        
        hashed_key = current_slot->hashed_key;
        owner = current_slot->header.owner;

        next_slot = current_slot->next_slot;

        if (IS_MARKED_AS_DELETED(next_slot))
            SOREN_LOGGER_INFO(*arg_logger, "[Deleted] > [{:3d}] Hash: {:12d}, Owned: {:4d}", size, hashed_key, owner);
        
        else if (IS_MARKED_AS_PROTECTED(next_slot))
            SOREN_LOGGER_INFO(*arg_logger, "[Protect] > [{:3d}] Hash: {:12d}, Owned: {:4d}", size, hashed_key, owner);

        else 
            SOREN_LOGGER_INFO(*arg_logger, "[       ] > [{:3d}] Hash: {:12d}, Owned: {:4d}", size, hashed_key, owner);

        size++;

        current_slot        = GET_UNMARKED_REFERENCE(next_slot);
    }

    SOREN_LOGGER_INFO(*arg_logger, "          > ~~~ TAIL ~~~");
    SOREN_LOGGER_INFO(*arg_logger, "-------- List Print End --------\n");
}


//
// main()
int main() {

    int HASHTABLE_SIZE      = (1 << 20);                   // 1 MB
    int HASHTABLE_NBUCKETS  = (HASHTABLE_SIZE >> 3);

    HASHTABLE_NBUCKETS = 32;

    char buffer[64] = { 0, };
    std::vector<int> sampled_idx{};

    struct List* bucket = nullptr, *reference_bucket = nullptr;
    soren::LocalSlot *prev, *next;

    soren::LocalSlot random_gen_slots[1024];
    soren::LocalSlot reference_slots[1024];

    soren::LocalSlot switch_slots[1024];

    std::memset(random_gen_slots, 0, sizeof(soren::LocalSlot) * 1024);
    std::memset(switch_slots, 0, sizeof(soren::LocalSlot) * 1024);

    uint32_t res = 0;
    bool is_samekey, is_success;

    //
    // Hash table instance initiate.
    soren::hash::LfHashTable hash_table(HASHTABLE_NBUCKETS, soren::localSlotHashComp);
    soren::hash::LfHashTable reference_ht(HASHTABLE_NBUCKETS, soren::localSlotHashComp);
    
    //
    // Initialize randomization device
    std::random_device random_device;
    std::mt19937 generator(random_device());

    SOREN_LOGGER_INFO(soren_lgr_ins_swt, "Insert tests.");

    for (int idx = 0; idx < 256; idx++) {

        generateRandomStr(generator, buffer, 15);
        random_gen_slots[idx].hashed_key            = hash_table.doHash(reinterpret_cast<void*>(buffer), 15);
        reference_slots[idx].hashed_key             = random_gen_slots[idx].hashed_key;

        random_gen_slots[idx + 256].hashed_key      = random_gen_slots[idx].hashed_key;
        random_gen_slots[idx + 512].hashed_key      = random_gen_slots[idx].hashed_key;
        random_gen_slots[idx + 768].hashed_key      = random_gen_slots[idx].hashed_key;

        random_gen_slots[idx].header.owner          = 1111;
        random_gen_slots[idx + 256].header.owner    = 2222;
        random_gen_slots[idx + 512].header.owner    = 3333;
        random_gen_slots[idx + 768].header.owner    = 4444;
    }

    for (int idx = 0; idx < 256; idx++) {

        is_samekey = reference_ht.doSearch(
            reference_slots[idx].hashed_key, &reference_slots[idx], &prev, &next);

        res = reference_slots[idx].hashed_key;

        if (!is_samekey) {
            is_success = reference_ht.doInsert(&reference_slots[idx], prev, next);
            if (!is_success)
                SOREN_LOGGER_INFO(soren_lgr_ins_swt, "Insert failed: Reference HT, for hash: {:12d}", res);
            else
                SOREN_LOGGER_INFO(soren_lgr_ins_swt, "Insert OK: Reference HT, for hash: {:12d}", res);
        }
    }

    bucket              = hash_table.debugGetBucket(random_gen_slots[0].hashed_key);
    reference_bucket    = reference_ht.debugGetBucket(reference_slots[0].hashed_key);

    // std::function<void(soren::LocalSlot*, int, soren::hash::LfHashTable&)> 
    auto worker_func = [](
        soren::LocalSlot* arg_rndslots, 
        int arg_off,
        soren::hash::LfHashTable& arg_ht) {
        
        int owner;
        bool is_samekey = false, is_success;
        uint32_t collided_hash = 0;

        soren::LocalSlot *prev, *next;

        for (int idx = arg_off; idx < (arg_off + 256); idx++) {

            collided_hash = arg_rndslots[idx].hashed_key;
            owner = arg_rndslots[idx].header.owner;

            is_samekey = arg_ht.doSearch(
                arg_rndslots[idx].hashed_key, &arg_rndslots[idx], &prev, &next);

            if (!is_samekey) {
                is_success = arg_ht.doInsert(&arg_rndslots[idx], prev, next);
                if (!is_success)
                    SOREN_LOGGER_INFO(soren_lgr_ins_swt, "Insert failed: Owner thread ({}), for hash: {:12d}", owner, collided_hash);
                else
                    SOREN_LOGGER_INFO(soren_lgr_ins_swt, "Insert OK: Owner thread ({}), for hash: {:12d}", owner, collided_hash);
            }
            else {
                is_success = arg_ht.doSwitch(&arg_rndslots[idx], prev, next);
                if (!is_success)
                    SOREN_LOGGER_INFO(soren_lgr_ins_swt, "Switch failed: Owner thread ({}), for hash: {:12d}", owner, collided_hash);
                else
                    SOREN_LOGGER_INFO(soren_lgr_ins_swt, "Switch OK: Owner thread ({}), for hash: {:12d}", owner, collided_hash);
            }
        }
    };

    // printAll(&soren_lgr_ins_swt, hash_table.debugGetBucket(0));  // Test for this bucket.

    std::thread worker_1(worker_func, 
        random_gen_slots, 0, std::ref(hash_table));
    std::thread worker_2(worker_func, 
        random_gen_slots, 256, std::ref(hash_table));
    std::thread worker_3(worker_func, 
        random_gen_slots, 512, std::ref(hash_table));
    std::thread worker_4(worker_func, 
        random_gen_slots, 768, std::ref(hash_table));

    worker_1.join();
    worker_2.join();
    worker_3.join();
    worker_4.join();

    printAll(&soren_lgr_ins_swt, bucket);

    SOREN_LOGGER_INFO(soren_lgr_ins_swt, "---- REFERENCE ----");
    printAll(&soren_lgr_ins_swt, reference_bucket);

    return 0;
}