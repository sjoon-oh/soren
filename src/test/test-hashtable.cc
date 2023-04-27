/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>
#include <random>

#include "../soren.hh"

//
// Test compare function
int32_t test_comp_func(void*, void*);

static soren::LoggerFileOnly soren_lgr("soren-hash", "soren-hashtable.test.log");


void generateRandomStr(std::mt19937& arg_gen, char* arg_buf, int arg_len) {

    std::uniform_int_distribution<int> distributor(33, 126);        // Printable ASCII range.

    for (int i = 0; i < arg_len; i++)
        arg_buf[i] = static_cast<char>(distributor(arg_gen));
}

void printAll(struct List* arg_list) {
    
    struct soren::LocalSlot* current_slot = arg_list->head.next_slot;

    uint32_t hashed_key;

    SOREN_LOGGER_INFO(soren_lgr, "---- List Print Start ----");
    while (current_slot != &arg_list->head) {
        
        hashed_key = current_slot->hashed_key;
        SOREN_LOGGER_INFO(soren_lgr, "    > Hashed: {}", hashed_key);

        current_slot = GET_UNMARKED_REFERENCE(current_slot->next_slot);
    }
    SOREN_LOGGER_INFO(soren_lgr, "---- List Print End ----\n\n");
}

//
// main()
int main() {

    int HASHTABLE_SIZE      = (1 << 20);                   // 1 MB
    int HASHTABLE_NBUCKETS  = (HASHTABLE_SIZE >> 3);

    HASHTABLE_NBUCKETS = 50;                                // Collision Test.

    char buffer[64] = { 0, };

    struct List* bucket;
    soren::LocalSlot test_slots[1024];
    soren::LocalSlot *prev, *next;

    std::memset(test_slots, 0, sizeof(soren::LocalSlot) * 1024);

    uint32_t res = 0;
    bool is_samekey = 0, is_success = 0;

    // Hash table instance initiate.
    soren::hash::LfHashTable hash_table(HASHTABLE_NBUCKETS, test_comp_func);

    SOREN_LOGGER_INFO(soren_lgr, "Hash table test start.");

    //
    // Initialize randomization device
    std::random_device random_device;
    std::mt19937 generator(random_device());

    for (int iter = 0; iter < 1000; iter++) {
        generateRandomStr(generator, buffer, 15);
        SOREN_LOGGER_INFO(soren_lgr, "Generated random key: {}", buffer);
        
        test_slots[iter].hashed_key = hash_table.doHash(reinterpret_cast<void*>(buffer), 15);
        res = test_slots[iter].hashed_key;

        SOREN_LOGGER_INFO(soren_lgr, "Hashed: {}", res);

        is_samekey = hash_table.doSearch(
            test_slots[iter].hashed_key, &test_slots[iter], &prev, &next);

        if (!is_samekey) {
            SOREN_LOGGER_INFO(soren_lgr, "Inserting a key...");

            is_success = hash_table.doInsert(&test_slots[iter], prev, next);

            if (!is_success)
                SOREN_LOGGER_INFO(soren_lgr, "Failed.");

            else {
                bucket = hash_table.debugGetBucket(res);
                printAll(bucket);
            }
        }
    }

    SOREN_LOGGER_INFO(soren_lgr, "Hash table test end.");

    return 0;
}

int32_t test_comp_func(void* arg_tar_a, void* arg_tar_b) {

    soren::LocalSlot* target_a = reinterpret_cast<soren::LocalSlot*>(arg_tar_a);
    soren::LocalSlot* target_b = reinterpret_cast<soren::LocalSlot*>(arg_tar_b);
    
    return target_a->hashed_key > target_b->hashed_key;
}