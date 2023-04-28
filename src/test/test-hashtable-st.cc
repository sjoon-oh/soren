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

static soren::LoggerFileOnly soren_lgr("soren-hash", "soren-hashtable-st.test.log");


void generateRandomStr(std::mt19937& arg_gen, char* arg_buf, int arg_len) {

    std::uniform_int_distribution<int> distributor(33, 126);        // Printable ASCII range.

    for (int i = 0; i < arg_len; i++)
        arg_buf[i] = static_cast<char>(distributor(arg_gen));
}


void printAll(struct List* arg_list) {
    
    struct soren::LocalSlot* current_slot = arg_list->head.next_slot;
    struct soren::LocalSlot* current_slot_raw = arg_list->head.next_slot;

    uint32_t hashed_key, front_val = 0;
    uint16_t owner;

    SOREN_LOGGER_INFO(soren_lgr, "-------- List Print Start --------");
    SOREN_LOGGER_INFO(soren_lgr, "           > ~ HEAD ~");

    while (current_slot != &arg_list->head) {
        
        hashed_key = current_slot->hashed_key;
        owner = current_slot->header.owner;

        if (IS_MARKED_AS_DELETED(current_slot_raw))
            SOREN_LOGGER_INFO(soren_lgr, " [Deleted] > Hashed: {}, Owned: {}", hashed_key, owner);
        
        else if (IS_MARKED_AS_PROTECTED(current_slot_raw))
            SOREN_LOGGER_INFO(soren_lgr, " [Protect] > Hashed: {}, Owned: {}", hashed_key, owner);

        else 
            SOREN_LOGGER_INFO(soren_lgr, "           > Hashed: {}, Owned: {}", hashed_key, owner);

        current_slot        = GET_UNMARKED_REFERENCE(current_slot->next_slot);
        current_slot_raw    = current_slot->next_slot;
    }

    SOREN_LOGGER_INFO(soren_lgr, "           > ~ TAIL ~");
    SOREN_LOGGER_INFO(soren_lgr, "-------- List Print End --------\n");
}


//
// main()
int main() {

    int HASHTABLE_SIZE      = (1 << 20);                   // 1 MB
    int HASHTABLE_NBUCKETS  = (HASHTABLE_SIZE >> 3);

    HASHTABLE_NBUCKETS = 50;                                // Collision Test.

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

    SOREN_LOGGER_INFO(soren_lgr, "Hash table test start.");
    SOREN_LOGGER_INFO(soren_lgr, "---- Single-threaded test start ----");

    SOREN_LOGGER_INFO(soren_lgr, "Observing single bucket...");
    
    //
    // Initialize randomization device
    std::random_device random_device;
    std::mt19937 generator(random_device());

    SOREN_LOGGER_INFO(soren_lgr, "Insert tests.");

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
                SOREN_LOGGER_INFO(soren_lgr, "Failed.");
                return -1;
            }

            if (bucket == nullptr)
                bucket = hash_table.debugGetBucket(res);

            if (bucket == hash_table.debugGetBucket(res)) {
                SOREN_LOGGER_INFO(soren_lgr, "Inserting random key: {}, hashed: {}", buffer, res);
                printAll(bucket);

                sampled_idx.push_back(iter);
                switch_slots[iter] = random_gen_slots[iter];
                switch_slots[iter].header.owner = 123;
            }
        }
    }

    SOREN_LOGGER_INFO(soren_lgr, "End of insert tests.");
    SOREN_LOGGER_INFO(soren_lgr, "Switch tests: Using inserted hash.");

    for (auto& idx: sampled_idx) {

        is_samekey = hash_table.doSearch(
            random_gen_slots[idx].hashed_key, &random_gen_slots[idx], &prev, &next);

        GET_TIMESTAMP((switch_slots[idx].timestamp));

        SOREN_LOGGER_INFO(soren_lgr, "Index({})", idx);
        printAll(bucket);

        if (is_samekey) {
            
            is_success = hash_table.doSwitch(&switch_slots[idx], prev, next);

            if (!is_success) {
                SOREN_LOGGER_INFO(soren_lgr, "Failed.");
                return -1;
            }

            res = switch_slots[idx].hashed_key;
            SOREN_LOGGER_INFO(soren_lgr, "Switched hashed key: {}\n", res);
        }
    }

    SOREN_LOGGER_INFO(soren_lgr, "End of switch tests.");
    SOREN_LOGGER_INFO(soren_lgr, "---- Single-threaded test end ----");

    SOREN_LOGGER_INFO(soren_lgr, "Hash table test end.");

    return 0;
}