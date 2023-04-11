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
        struct hartebeest::Mr* searchMr(uint32_t, uint32_t, uint32_t);
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
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "PD({}) created.", arg_pd_id);
    return HB_CONFIGURATOR->doRegisterPd2(arg_pd_id);
}

uint8_t* soren::hbwrapper::allocateBuffer(size_t arg_len, int arg_align) {
    return HB_CONFIGURATOR->doAllocateBuffer2(arg_len, arg_align);
}

int soren::hbwrapper::registerMr(uint32_t arg_pd_id, uint32_t arg_mr_id, uint8_t* buf, size_t arg_len) {
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "MR({}) => PD({}).", arg_mr_id, arg_pd_id);
    return HB_CONFIGURATOR->doCreateAndRegisterMr2(arg_pd_id, arg_mr_id, buf, arg_len);
}

int soren::hbwrapper::registerRcQp(
        uint32_t    arg_pd_id,
        uint32_t    arg_qp_id,
        uint32_t    arg_send_cq_id,
        uint32_t    arg_recv_cq_id
    ) {

        int ret = HB_CONFIGURATOR->doCreateAndRegisterCq2(arg_send_cq_id);
        ret = HB_CONFIGURATOR->doCreateAndRegisterCq2(arg_recv_cq_id);

        ret = HB_CONFIGURATOR->doCreateAndRegisterRcQp2(arg_pd_id, arg_qp_id, arg_send_cq_id, arg_recv_cq_id);

        SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "QP({}), CQs({}/{}) => PD({})",
            arg_qp_id, arg_send_cq_id, arg_recv_cq_id, arg_pd_id);

        ret = HB_CONFIGURATOR->doInitQp2(arg_qp_id);        
        return ret;
};

int soren::hbwrapper::exchangeRdmaConfigs() {
    
    HB_CONFIGURATOR->doExportAll2(HB_EXCHANGER->getThisNodeId(), "./config/local-config.json");
    int ret = HB_EXCHANGER->setThisNodeConf("./config/local-config.json");

    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "Exchanging RDMA configs (TCP)...");
    ret = HB_EXCHANGER->doExchange();

    if (ret == true)
        SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > OK.");
    else {
        SOREN_LOGGER_ERROR(hb_hbwrapper_lgr, " > Fail.");
        abort();
    }

    sleep(HB_DELAY);

    std::call_once(
        HB_NETCTX_FLAG, [](Logger& arg_lgr, hartebeest::RdmaNetworkContext& arg_ctx){
            
            SOREN_LOGGER_INFO(arg_lgr, "Exporting RDMA network context...");
            arg_ctx = HB_EXCHANGER->doExportNetworkContext();

            sleep(HB_DELAY);

        }, std::ref(hb_hbwrapper_lgr), std::ref(NETWORK_RDMA_CONTEXT)
    );

    if (NETWORK_RDMA_CONTEXT.num_nodes != HB_EXCHANGER->getNumOfPlayers()) {
        SOREN_LOGGER_ERROR(hb_hbwrapper_lgr, " > Exported context mismatch.");
        return -1;
    }

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

    // Search remote node.
    for (int node_idx = 0; node_idx < NETWORK_RDMA_CONTEXT.num_nodes; node_idx++) {
        
        nctx = &(NETWORK_RDMA_CONTEXT.nodes[node_idx]);
        if (nctx->nid == arg_nid)
            break;
    }

    // Search Protection Domain
    for (int pd_idx = 0; pd_idx < nctx->num_pds; pd_idx++) {

        pd = &(nctx->pds[pd_idx]);
        if (pd->pd_id == arg_pd_id)
            break; 
    }

    // Search Queue Pair
    for (int qp_idx = 0; qp_idx < pd->num_qps; qp_idx++) {

        qp = &(pd->qps[qp_idx]);
        if (qp->qp_id == arg_qp_id)
            break; 
    }

    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, " > Node {} QP({}): QPN({}), PID({}), PLID({})", nctx->nid, qp->qp_id, qp->qpn, qp->pid, qp->plid);

    return qp;
}

hartebeest::Mr* soren::hbwrapper::searchMr(uint32_t arg_nid, uint32_t arg_pd_id, uint32_t arg_mr_id) {

    struct hartebeest::Pd*          pd;
    struct hartebeest::Mr*          mr;
    struct hartebeest::NodeContext* nctx;

    // Search remote node.
    for (int node_idx = 0; node_idx < NETWORK_RDMA_CONTEXT.num_nodes; node_idx++) {
        
        nctx = &(NETWORK_RDMA_CONTEXT.nodes[node_idx]);
        if (nctx->nid == arg_nid)
            break;
    }

    // Search Protection Domain
    for (int pd_idx = 0; pd_idx < nctx->num_pds; pd_idx++) {

        pd = &(nctx->pds[pd_idx]);
        if (pd->pd_id == arg_pd_id)
            break; 
    }

    // Search Queue Pair
    for (int mr_idx = 0; mr_idx < pd->num_mrs; mr_idx++) {

        mr = &(pd->mrs[mr_idx]);
        if (mr->mr_id == arg_mr_id)
            break; 
    }

    return mr;
}

int soren::hbwrapper::connectRcQps(
        uint32_t arg_qp_id,         // Only QP ID will be enough, since all QP IDs are unique.
        uint32_t arg_remote_nid,    // Remote Node ID
        uint32_t arg_remote_pd_id,  // Remote Protection Domain ID
        uint32_t arg_remote_qp_id   // Remote Queue Pair ID
        ) {
    SOREN_LOGGER_INFO(hb_hbwrapper_lgr, "Connecting QP({}) to QP({})", arg_qp_id, arg_remote_qp_id);
    
    hartebeest::Qp* qp = searchQp(arg_remote_nid, arg_remote_pd_id, arg_remote_qp_id);
    return HB_CONFIGURATOR->doConnectRcQp2(arg_qp_id, qp->pid, qp->qpn, qp->plid);
}

int soren::hbwrapper::getThisNodeId() {
    return HB_EXCHANGER->getThisNodeId();
}

int soren::hbwrapper::getNumPlayers() {
    return HB_EXCHANGER->getNumOfPlayers();
}

struct ibv_mr* soren::hbwrapper::getLocalMr(uint32_t arg_id) {
    struct ibv_mr* local_mr = HB_CONFIGURATOR->getMr(arg_id);

    if (local_mr == nullptr)
        SOREN_LOGGER_ERROR(hb_hbwrapper_lgr, "Local MR({}) not registered?", arg_id);
    
    return local_mr;
}

struct ibv_qp* soren::hbwrapper::getLocalQp(uint32_t arg_id) {

    struct ibv_qp* local_qp = HB_CONFIGURATOR->getQp(arg_id);

    if (local_qp == nullptr)
        SOREN_LOGGER_ERROR(hb_hbwrapper_lgr, "Local QP({}) not registered?", arg_id);
    
    return local_qp;
}

struct ibv_mr* soren::hbwrapper::getRemoteMinimalMr(uint32_t arg_nid, uint32_t arg_pd_id, uint32_t arg_id) {
     
    auto remote_mr = searchMr(arg_nid, arg_pd_id, arg_id);

    // Make new.
    struct ibv_mr* mr = new (struct ibv_mr);
    std::memset(mr, 0, sizeof(struct ibv_mr));

    // Fill minimals.
    mr->addr = reinterpret_cast<void*>(remote_mr->addr);
    mr->length = remote_mr->length;
    mr->rkey = remote_mr->rkey;

    return mr;
}

// Not wrapped, but pure.
int soren::rdmaPost(
        enum ibv_wr_opcode      arg_opc,
        struct ibv_qp*          arg_qp,
        uintptr_t               arg_addr,
        uint32_t                arg_size, 
        uint32_t                arg_lk,
        uintptr_t               arg_remote_addr,
        uint32_t                arg_remote_rk
    ) {

    struct ibv_send_wr    work_req;
    struct ibv_sge        sg_elem;
    struct ibv_send_wr*   bad_work_req = nullptr;

    std::memset(&sg_elem, 0, sizeof(sg_elem));
    std::memset(&work_req, 0, sizeof(work_req));

    sg_elem.addr        = arg_addr;
    sg_elem.length      = arg_size;
    sg_elem.lkey        = arg_lk;

    work_req.wr_id      = 0;
    work_req.num_sge    = 1;
    work_req.opcode     = arg_opc;
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

int soren::waitRdmaSend(struct ibv_qp* arg_local_qp) {

    struct ibv_wc   work_completion;
    int             nwc;

    do {
        nwc = ibv_poll_cq(arg_local_qp->send_cq, 1, &work_completion);
    } while (nwc == 0);

    switch (work_completion.status) {
        case IBV_WC_SUCCESS:
            return 0;
        default:
            return work_completion.status;
    }
}