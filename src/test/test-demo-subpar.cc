/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>
#include <unistd.h>

#include "../soren.hh"

#include <mutex>
std::mutex mem_corrupt_mut;

int main() {

    soren::initSoren(10, 2);

    std::string test_payload("Literature adds to reality, it does not simply describe it. \
It enriches the necessary competencies that daily life requires and provides; and in \
this respect, it irrigates the deserts that our lives have already become.");

    char* target = const_cast<char*>(test_payload.c_str());

    for (int i = 0; i < 10; i++) {
        for (int off = 0; off < test_payload.size(); off += 31) {
            
            soren::getReplicator()->doPropose(reinterpret_cast<uint8_t*>(target + off), 32, (uint16_t)(*(target + off)));
        }
    }
    
    sleep(20);

    soren::cleanSoren();
    
    return 0;
}