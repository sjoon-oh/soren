/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>
#include <random>

#include <vector>
#include <cstdio>

#include "../soren.hh"

static soren::LoggerFileOnly soren_lgr_insert("hash-insert", "soren-hashtable-st-insert.test.log");
static soren::LoggerFileOnly soren_lgr_switch("hash-switch", "soren-hashtable-st-switch.test.log");
static soren::LoggerFileOnly soren_lgr_delete("hash-delete", "soren-hashtable-st-delete.test.log");

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

    HASHTABLE_NBUCKETS = 128;

    char buffer[64] = { 0, };
    std::vector<int> sampled_idx{};

    struct List* bucket = nullptr;

    soren::LocalSlot random_gen_slots[1024];
    soren::LocalSlot switch_slots[1024];
    soren::LocalSlot *prev, *next;

    std::memset(random_gen_slots, 0, sizeof(soren::LocalSlot) * 1024);
    std::memset(switch_slots, 0, sizeof(soren::LocalSlot) * 1024);

    uint32_t res = 0;
    bool is_samekey = 0, is_success = 0;

    // Hash table instance initiate.
    soren::hash::LfHashTable hash_table(HASHTABLE_NBUCKETS, soren::localSlotHashComp);
    
    //
    // Initialize randomization device
    std::random_device random_device;
    std::mt19937 generator(random_device());

    SOREN_LOGGER_INFO(soren_lgr_insert, "Insert tests.");

    for (int iter = 0; iter < 1024; iter++) {
        generateRandomStr(generator, buffer, 15);
        
        random_gen_slots[iter].hashed_key = hash_table.doHash(reinterpret_cast<void*>(buffer), 15);
        random_gen_slots[iter].header.owner = 0;
        res = random_gen_slots[iter].hashed_key;

        // SOREN_LOGGER_INFO(soren_lgr, "Random Key: {}, Hashed: {}", buffer, res);

        is_samekey = hash_table.doSearch(
            random_gen_slots[iter].hashed_key, &random_gen_slots[iter], &prev, &next);

        GET_TIMESTAMP((random_gen_slots[iter].timestamp));

        if (!is_samekey) {
            
            is_success = hash_table.doInsert(&random_gen_slots[iter], prev, next);

            if (!is_success) {
                SOREN_LOGGER_INFO(soren_lgr_insert, "Failed.");
                return -1;
            }

            if (bucket == nullptr)
                bucket = hash_table.getBucket(res);

            if (bucket == hash_table.getBucket(res)) {
                SOREN_LOGGER_INFO(soren_lgr_insert, "Inserting random key: {}, hashed: {}", buffer, res);
                printAll(&soren_lgr_insert, bucket);

                sampled_idx.push_back(iter);
                switch_slots[iter] = random_gen_slots[iter];
                switch_slots[iter].header.owner = 123;
            }
        }
    }

    SOREN_LOGGER_INFO(soren_lgr_insert, "End of insert tests.");
    SOREN_LOGGER_INFO(soren_lgr_switch, "Switch tests: Using inserted hash.");

    for (auto& idx: sampled_idx) {

        is_samekey = hash_table.doSearch(
            random_gen_slots[idx].hashed_key, &random_gen_slots[idx], &prev, &next);

        GET_TIMESTAMP((switch_slots[idx].timestamp));
        res = switch_slots[idx].hashed_key;

        SOREN_LOGGER_INFO(soren_lgr_switch, "After search of index({}), hashed key: {}", idx, res);
        printAll(&soren_lgr_switch, bucket);

        if (is_samekey) {
            
            is_success = hash_table.doSwitch(&switch_slots[idx], prev, next);

            if (!is_success) {
                SOREN_LOGGER_INFO(soren_lgr_switch, "Failed.");
                return -1;
            }

            res = switch_slots[idx].hashed_key;

            SOREN_LOGGER_INFO(soren_lgr_switch, "After switch of hashed key: {}", res);
            printAll(&soren_lgr_switch, bucket);
        }
    }

    SOREN_LOGGER_INFO(soren_lgr_switch, "End of switch tests.");

    SOREN_LOGGER_INFO(soren_lgr_delete, "Start of delete tests.");
    // for (auto& idx: sampled_idx) {
    //     res = switch_slots[idx].hashed_key;
    //     hash_table.doDelete(&switch_slots[idx]);

    //     SOREN_LOGGER_INFO(soren_lgr_delete, "After delete: {}", res);
    //     printAll(&soren_lgr_delete, bucket);
    // }

    hash_table.doCleanups(bucket);
    
    SOREN_LOGGER_INFO(soren_lgr_delete, "After cleanups");
    printAll(&soren_lgr_delete, bucket);


    // SOREN_LOGGER_INFO(soren_lgr_delete, "After delete:");
    // printAll(&soren_lgr_delete, bucket);

    SOREN_LOGGER_INFO(soren_lgr_delete, "End of delete tests.");


    return 0;
}