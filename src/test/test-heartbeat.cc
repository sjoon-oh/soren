/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>
#include <thread>

#include "../soren.hh"

static soren::Logger soren_lgr("SOREN", "soren-heartbeat.test.log");
static soren::Logger hb_lgr("HARTEBEEST", "hartebeest.log");

int main() {
    
    soren::HeartbeatLocalRunner hb_runner(10000);

    SOREN_LOGGER_INFO(soren_lgr, "Soren heartbeat test run.");
    



    SOREN_LOGGER_INFO(soren_lgr, "Soren heartbeat test end.");

clean_up:
    return 0;
}