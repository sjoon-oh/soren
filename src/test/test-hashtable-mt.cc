/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>
#include <random>

#include "../soren.hh"

static soren::LoggerFileOnly soren_lgr("soren-hash", "soren-hashtable-mt.test.log");


void printAll(struct List* arg_list) {
    
    struct soren::LocalSlot* current_slot = arg_list->head.next_slot;

    uint32_t hashed_key;

    SOREN_LOGGER_INFO(soren_lgr, "---- List Print Start ----");
    SOREN_LOGGER_INFO(soren_lgr, "    > HEAD");

    while (current_slot != &arg_list->head) {
        
        hashed_key = current_slot->hashed_key;
        SOREN_LOGGER_INFO(soren_lgr, "    > Hashed: {}", hashed_key);

        current_slot = GET_UNMARKED_REFERENCE(current_slot->next_slot);
    }

    SOREN_LOGGER_INFO(soren_lgr, "    > TAIL");
    SOREN_LOGGER_INFO(soren_lgr, "---- List Print End ----\n");
}


int main() {

    















    return 0;
}

