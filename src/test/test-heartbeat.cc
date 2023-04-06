/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <unistd.h>

#include <string>
#include <thread>

#include "../soren.hh"

static soren::Logger soren_lgr("SOREN", "soren-heartbeat.test.log");
static soren::Logger hb_lgr("HARTEBEEST", "hartebeest.log");

int main() {
    
    soren::HeartbeatLocalRunner hb_runner(10000);

    SOREN_LOGGER_INFO(soren_lgr, "Soren heartbeat test run.");
    
    hb_runner.doLaunchRunner();

    for (int count = 0; count < 10; count++) {

        SOREN_LOGGER_INFO(soren_lgr, " > Heartbeat peek: {}", hb_runner.doPeek());
        usleep(100000);
    }

    hb_runner.doKillRunner();

    SOREN_LOGGER_INFO(soren_lgr, "Soren heartbeat test end.");

clean_up:
    return 0;
}