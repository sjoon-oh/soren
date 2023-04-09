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

    // Do not mess up with multiple initializations and clean ups.
    std::once_flag HB_INIT_FLAG;    
    std::once_flag HB_NETCTX_FLAG;
    std::once_flag HB_CLEAN_FLAG;

    std::unique_ptr<hartebeest::RdmaConfigurator>       HB_CONFIGURATOR;
    std::unique_ptr<hartebeest::ConfigFileExchanger>    HB_EXCHANGER;

    const int HB_DELAY = 2;

    namespace hbwrapper {
        struct hartebeest::RdmaNetworkContext           NETWORK_RDMA_CONTEXT = {0, nullptr};

        struct hartebeest::Qp* searchQp(uint32_t, uint32_t, uint32_t);
    }
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

    if (NETWORK_RDMA_CONTEXT.nodes != nullptr)
        HB_EXCHANGER->doCleanNetworkContext(NETWORK_RDMA_CONTEXT);

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

    if (HB_EXCHANGER->doReadConfigFile("./config/eth-config.json") == false) {
        SOREN_LOGGER_ERROR(hb_hbwrapper_lgr, " > Failed.");
        return false;
    }

    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > OK");
    return true;
}

int soren::hbwrapper::registerPd(uint32_t arg_pd_id) {
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "PD registration...");
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > PD({}) registered.", arg_pd_id);
    return HB_CONFIGURATOR->doRegisterPd2(arg_pd_id);
}

uint8_t* soren::hbwrapper::allocateBuffer(size_t arg_len, int arg_align) {
    return HB_CONFIGURATOR->doAllocateBuffer2(arg_len, arg_align);
}

int soren::hbwrapper::registerMr(uint32_t arg_pd_id, uint32_t arg_mr_id, uint8_t* buf, size_t arg_len) {
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "MR registration...");
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > MR({}) registered to PD({}).", arg_mr_id, arg_pd_id);
    return HB_CONFIGURATOR->doCreateAndRegisterMr2(arg_pd_id, arg_mr_id, buf, arg_len);
}

int soren::hbwrapper::registerRcQp(
        uint32_t    arg_pd_id,
        uint32_t    arg_qp_id,
        uint32_t    arg_send_cq_id,
        uint32_t    arg_recv_cq_id
    ) {
        SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "rcQP registration...");

        int ret = HB_CONFIGURATOR->doCreateAndRegisterCq2(arg_send_cq_id);
        ret = HB_CONFIGURATOR->doCreateAndRegisterCq2(arg_recv_cq_id);

        ret = HB_CONFIGURATOR->doCreateAndRegisterRcQp2(arg_pd_id, arg_qp_id, arg_send_cq_id, arg_recv_cq_id);

        SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > QP({}) with send/recv CQs({}/{}) registered to PD({})",
            arg_qp_id, arg_send_cq_id, arg_recv_cq_id, arg_pd_id);

        ret = HB_CONFIGURATOR->doInitQp2(arg_qp_id);
        SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > QP({}) transit to INIT.",
            arg_qp_id, arg_send_cq_id, arg_recv_cq_id, arg_pd_id);
        
        return ret;
};

int soren::hbwrapper::exchangeRdmaConfigs() {
    
    HB_CONFIGURATOR->doExportAll2(HB_EXCHANGER->getThisNodeId(), "./config/local-config.json");
    int ret = HB_EXCHANGER->setThisNodeConf("./config/local-config.json");
    ret = HB_EXCHANGER->doExchange();

    sleep(HB_DELAY);

    std::call_once(
        HB_NETCTX_FLAG, [](Logger& arg_lgr, hartebeest::RdmaNetworkContext& arg_ctx){
            
            SOREN_LOGGER_INFO(arg_lgr, "Exporting RDMA network context...");
            arg_ctx = HB_EXCHANGER->doExportNetworkContext();

            sleep(HB_DELAY);

        }, std::ref(hb_hbwrapper_lgr), std::ref(NETWORK_RDMA_CONTEXT)
    );

    SOREN_LOGGER_INFO(
        hb_hbwrapper_lgr, " > OK. Exported context: {} nodes in total found.", 
        NETWORK_RDMA_CONTEXT.num_nodes
    );
    
    return ret;
}

hartebeest::Qp* soren::hbwrapper::searchQp(uint32_t arg_nid, uint32_t arg_pd_id, uint32_t arg_qp_id) {

    struct hartebeest::Pd*          pd;
    struct hartebeest::Qp*          qp;
    struct hartebeest::NodeContext* nctx;
    
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "Search:");

    // Search remote node.
    for (int node_idx = 0; node_idx < NETWORK_RDMA_CONTEXT.num_nodes; node_idx++) {
        
        nctx = &(NETWORK_RDMA_CONTEXT.nodes[node_idx]);
        if (nctx->nid == arg_nid)
            break;
    }

    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > Node {} context: {} PDs", nctx->nid, nctx->num_pds);

    // Search Protection Domain
    for (int pd_idx = 0; pd_idx < nctx->num_pds; pd_idx++) {

        pd = &(nctx->pds[pd_idx]);
        if (pd->pd_id == arg_pd_id)
            break; 
    }

    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > Node {} PD({}) context: {} MRs, {} QPs", nctx->nid, pd->pd_id, pd->num_mrs, pd->num_qps);

    // Search Queue Pair
    for (int qp_idx = 0; qp_idx < pd->num_qps; qp_idx++) {

        qp = &(pd->qps[qp_idx]);
        if (qp->qp_id == arg_qp_id)
            break; 
    }

    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > Node {} QP({}) context: QPN({}), PID({}), PLID({})", nctx->nid, qp->qp_id, qp->qpn, qp->pid, qp->plid);

    return qp;
}

int soren::hbwrapper::connectRcQps(
        uint32_t arg_qp_id,         // Only QP ID will be enough, since all QP IDs are unique.
        uint32_t arg_remote_nid,    // Remote Node ID
        uint32_t arg_remote_pd_id,  // Remote Protection Domain ID
        uint32_t arg_remote_qp_id   // Remote Queue Pair ID
        ) {
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "Connecting QP of Node {}, searching...", arg_remote_nid);
    
    hartebeest::Qp* qp = searchQp(arg_remote_nid, arg_remote_pd_id, arg_remote_qp_id);
    return HB_CONFIGURATOR->doConnectRcQp2(arg_qp_id, qp->pid, qp->qpn, qp->plid);
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

// Not wrapped, but pure.
int soren::rdmaWrite(
        struct ::ibv_qp*    arg_qp,
        uintptr_t           arg_addr,
        uint32_t            arg_size, 
        uint32_t            arg_lk,
        uintptr_t           arg_remote_addr,
        uint32_t            arg_remote_rk
    ) {

    struct ::ibv_send_wr    work_req;
    struct ::ibv_sge        sg_elem;
    struct ::ibv_send_wr*   bad_work_req = nullptr;

    std::memset(&sg_elem, 0, sizeof(sg_elem));
    std::memset(&work_req, 0, sizeof(work_req));

    sg_elem.addr        = arg_addr;
    sg_elem.length      = arg_size;
    sg_elem.lkey        = arg_lk;

    work_req.wr_id      = 0;
    work_req.num_sge    = 1;
    work_req.opcode     = IBV_WR_RDMA_WRITE;
    work_req.send_flags = IBV_SEND_SIGNALED;
    work_req.wr_id      = 0;
    work_req.sg_list    = &sg_elem;
    work_req.next       = nullptr;

    work_req.wr.rdma.remote_addr    = arg_remote_addr;
    work_req.wr.rdma.rkey           = arg_remote_rk;

    int ret = ibv_post_send(arg_qp , &work_req, &bad_work_req);
    
    if (bad_work_req != nullptr) return -1;
    if (ret != 0) return -1;

    return 0;
}

int soren::rdmaRead() {

    return 0;
}

