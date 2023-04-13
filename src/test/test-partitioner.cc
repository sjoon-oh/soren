/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <unistd.h>

#include <string>
#include <thread>

#include "../soren.hh"

static soren::Logger soren_lgr("SOREN", "soren-partitioner.test.log");

int main() {
    
    const int num_players = 5;

    SOREN_LOGGER_INFO(soren_lgr, "Soren partitioner test run.");

    soren::Partitioner ruler(num_players, 10);
    char* some_key = "AAAABBBBAAAABBBB";

    SOREN_LOGGER_INFO(soren_lgr, "Test key: {}, Target: {}", some_key, 0x41414141);

    SOREN_LOGGER_INFO(soren_lgr, "Owner: {}, Sim {}", 
        ruler.doGetOwner(some_key), 0x41414141 % 5);

    SOREN_LOGGER_INFO(soren_lgr, "Now, iteration of 10 times.");
    for (int iter = 0; iter < 10; iter++) {
        ruler.doDecrRange();
        SOREN_LOGGER_INFO(soren_lgr, "Range decreased, Owner: {}", 
            ruler.doGetOwner(some_key));
    }

    SOREN_LOGGER_INFO(soren_lgr, "Let player node 0 dead.");
    ruler.setPlayerDead(0);

    SOREN_LOGGER_INFO(soren_lgr, "Ignoring dead owner: {}", 
        ruler.doGetOwner(some_key));


    SOREN_LOGGER_INFO(soren_lgr, "Soren partitioner test end.");

clean_up:
    return 0;
}