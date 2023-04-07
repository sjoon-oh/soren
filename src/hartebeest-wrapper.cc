/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>

#include "rdma-conf.hpp"

#include "logger.hh"
#include "hartebeest-wrapper.hh"

static soren::Logger hb_hbwrapper_lgr("HB-hbwrapper", "hb-hbwrapper.log");

namespace soren {

    std::once_flag HB_INIT_FLAG;    // Do not mess up with multiple initializations and clean ups.
    std::once_flag HB_CLEAN_FLAG;

    std::unique_ptr<hartebeest::RdmaConfigurator>       HB_CONFIGURATOR;
    std::unique_ptr<hartebeest::ConfigFileExchanger>    HB_EXCHANGER;
}

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
        HB_CLEAN_FLAG, [](Logger& arg_lgr){
            
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
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "Protection Domain ID[{}] registered.", arg_pd_id);
    return HB_CONFIGURATOR->doRegisterPd2(arg_pd_id);
}

uint8_t* soren::hbwrapper::allocateBuffer(size_t arg_len, int arg_align) {
    return HB_CONFIGURATOR->doAllocateBuffer2(arg_len, arg_align);
}

int soren::hbwrapper::registerMr(uint32_t arg_pd_id, uint32_t arg_mr_id, uint8_t* buf, size_t arg_len) {
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "Memory Region ID[{}] registered to PD ID[{}].", arg_mr_id, arg_pd_id);
    return HB_CONFIGURATOR->doCreateAndRegisterMr2(arg_pd_id, arg_mr_id, buf, arg_len);
}

int soren::hbwrapper::registerRcQp(
        uint32_t    arg_pd_id,
        uint32_t    arg_qp_id,
        uint32_t    arg_send_cq_id,
        uint32_t    arg_recv_cq_id
    ) {
        SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "Creating RC QP ID[{}] with send/recv CQs ID[{}/{}] to PD ID[{}]",
            arg_qp_id, arg_send_cq_id, arg_recv_cq_id, arg_pd_id);

        int ret = HB_CONFIGURATOR->doCreateAndRegisterCq2(arg_send_cq_id);
        ret = HB_CONFIGURATOR->doCreateAndRegisterCq2(arg_recv_cq_id);

        ret = HB_CONFIGURATOR->doCreateAndRegisterRcQp2(arg_pd_id, arg_qp_id, arg_send_cq_id, arg_recv_cq_id);
        return ret;
};

int soren::hbwrapper::exchangeRdmaConfigs() {
    
    int ret = HB_CONFIGURATOR->doExportAll2(HB_EXCHANGER->getThisNodeId(), "./config/local-config.json");
    ret = HB_EXCHANGER->setThisNodeConf("./config/local-config.json");
    ret = HB_EXCHANGER->doExchange();

    return ret;
}

int soren::hbwrapper::getThisNodeId() {
    return HB_EXCHANGER->getThisNodeId();
}

int soren::hbwrapper::getNumPlayers() {
    return HB_EXCHANGER->getNumOfPlayers();
}

struct ::ibv_mr* soren::hbwrapper::getMr(uint32_t arg_id) {
    return HB_CONFIGURATOR->getMr(arg_id);
}

struct ::ibv_qp* soren::hbwrapper::getQp(uint32_t arg_id) {
    HB_CONFIGURATOR->getQp(arg_id);
}

