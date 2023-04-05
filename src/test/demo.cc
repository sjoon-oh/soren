/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>

#include "rdma-conf.hpp"
#include "../soren.hh"

static soren::Logger soren_lgr("SOREN", "soren-demo.run.log");
static soren::Logger hb_lgr("HARTEBEEST", "hartebeest.log");

int main() {
    
    hartebeest::RdmaConfigurator        confr;
    hartebeest::ConfigFileExchanger     exchr;

    SOREN_LOGGER_INFO(soren_lgr, "Soren demo run.");

    if (exchr.doReadConfigFile("./config/network-config.json") == false) {
        SOREN_LOGGER_ERROR(hb_lgr, "Exchanger cannot open the file.");
        goto clean_up;
    }

    SOREN_LOGGER_INFO(hb_lgr, "Exchanger opened the file.");
    SOREN_LOGGER_INFO(hb_lgr, "RdmaConfigurator start.");

    if (confr.doInitDevice2() != 0)
        SOREN_LOGGER_ERROR(hb_lgr, "RdmaConfigurator cannot initialize HCA device.");

    SOREN_LOGGER_INFO(hb_lgr, "RdmaConfigurator end.");

    SOREN_LOGGER_INFO(soren_lgr, "Soren demo end.");


clean_up:
    return 0;
}