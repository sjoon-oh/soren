/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>

#include "../soren.hh"
#include "../hartebeest-wrapper.hh"

static soren::Logger soren_lgr("SOREN-CONN-TEST", "soren-connector.test.log");
static soren::Logger hb_lgr("HARTEBEEST", "hartebeest.log");

int main() {

    SOREN_LOGGER_INFO(soren_lgr, "Soren connector test run.");

    SOREN_LOGGER_INFO(soren_lgr, "Hartebeest initialization first run.");
    soren::hbwrapper::initHartebeest();

    SOREN_LOGGER_INFO(soren_lgr, "Hartebeest initialization second run.");
    soren::hbwrapper::initHartebeest();

    soren::hbwrapper::initRdmaConfigurator();
    if (!soren::hbwrapper::initConfigFileExchanger())
        goto clean_up;

    









    SOREN_LOGGER_INFO(soren_lgr, "Soren connector test end.");

clean_up:

    soren::hbwrapper::cleanHartebeest();

    return 0;
}