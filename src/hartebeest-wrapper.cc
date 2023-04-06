/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>

#include "logger.hh"
#include "hartebeest-wrapper.hh"

static soren::Logger hb_hbwrapper_lgr("HB-hbwrapper", "hb-hbwrapper.log");

void soren::hbwrapper::initHartebeest() {

    std::call_once(
        HB_INIT_FLAG, [](Logger& arg_lgr){
            
            SOREN_LOGGER_INFO(arg_lgr, "Initializing Hartebeest module.");

            HB_CONFIGURATOR.reset(new hartebeest::RdmaConfigurator());
            HB_EXCHANGER.reset(new hartebeest::ConfigFileExchanger());

        }, std::ref(hb_hbwrapper_lgr)
    );
}

void soren::hbwrapper::cleanHartebeest() {

    std::call_once(
        HB_INIT_FLAG, [](Logger& arg_lgr){
            
            SOREN_LOGGER_INFO(arg_lgr, "Cleaning up Hartebeest module.");

            HB_CONFIGURATOR.release();
            HB_EXCHANGER.release();

        }, std::ref(hb_hbwrapper_lgr)
    );
}

void soren::hbwrapper::initRdmaConfigurator() {

    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "RdmaConfigurator setup sequence start.");
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > Getting HCA Device...");

    if (HB_CONFIGURATOR->doInitDevice2() != 0)
        SOREN_LOGGER_ERROR(hb_hbwrapper_lgr, " > Failed.");

    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > OK");
}

bool soren::hbwrapper::initConfigFileExchanger() {

    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "ConfigFileExchanger setup sequence start.");

    if (HB_EXCHANGER->doReadConfigFile("./config/network-config.json") == false) {
        SOREN_LOGGER_ERROR(hb_hbwrapper_lgr, " > Failed.");
        return false;
    }

    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > OK");
    return true;
}

int soren::hbwrapper::registerPd(uint32_t arg_pd_id) {
    return HB_CONFIGURATOR->doRegisterPd2(arg_pd_id);
}

uint8_t* soren::hbwrapper::allocateBuffer(size_t arg_len, int arg_align) {
    return HB_CONFIGURATOR->doAllocateBuffer2(arg_len, arg_align);
}

int soren::hbwrapper::registerMr(uint32_t arg_pd_id, uint32_t arg_mr_id, uint8_t* buf, size_t arg_len) {
    return HB_CONFIGURATOR->doCreateAndRegisterMr2(arg_pd_id, arg_mr_id, buf, arg_len);
}

int soren::hbwrapper::registerRcQp(
        uint32_t    arg_pd_id,
        uint32_t    arg_qp_id,
        uint32_t    arg_send_cq_id,
        uint32_t    arg_recv_cq_id
    ) {
        return HB_CONFIGURATOR->doCreateAndRegisterRcQp2(arg_pd_id, arg_qp_id, arg_send_cq_id, arg_recv_cq_id);
};

int soren::hbwrapper::exportLocalRdmaConfig(int arg_this_node_id) {
    return HB_CONFIGURATOR->doExportAll2(arg_this_node_id, "./config/local-config.json");
}

bool soren::hbwrapper::getReadyForExchange() {
    return HB_EXCHANGER->setThisNodeConf("./config/local-config.json");
};

int soren::hbwrapper::exchangeRdmaConfigs() {
    return HB_EXCHANGER->doExchange();
}