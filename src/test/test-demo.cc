/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>
#include "../soren.hh"

int main() {

    soren::initSoren();

    char buffer[32] = { 0, };
    std::string test_payload("The past can hurt. But from the way I see it, you can either run from it, or learn from it.");

    for (int off = 0; off < test_payload.size(); off += 31) {
        std::memset(buffer, 0, 32);
        std::memcpy(buffer, test_payload.c_str() + off, 31);
        
        soren::getReplicator()->doPropose(reinterpret_cast<uint8_t*>(buffer), 32);
    }
    
    sleep(10);

    soren::cleanSoren();
    
    return 0;
}