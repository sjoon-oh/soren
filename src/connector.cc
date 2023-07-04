/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "hartebeest-wrapper.hh"
#include "connector.hh"
#include "player.hh"

#include "commons.hh"

soren::Connector::Connector() {
    hbwrapper::initHartebeest();
    hbwrapper::initRdmaConfigurator();
    hbwrapper::initConfigFileExchanger();

    nplayers = hbwrapper::getNumPlayers();
    node_id = hbwrapper::getThisNodeId();

    soren::hbwrapper::registerPd(COMMON_PD);
    
    //
    // Create Mr Qps for Receiver: Replayer.
    uint8_t* buf_addr = nullptr;

    for (int pl_id = 0; pl_id < nplayers; pl_id++) {
        // for (int sp = 0; sp < arg_subpar; sp++) {

        //     buf_addr = hbwrapper::allocateBuffer(BUF_SIZE, 64);
        //     if (hbwrapper::registerMr(COMMON_PD, GET_MR_GLOBAL(pl_id, sp), buf_addr, BUF_SIZE) != 0) {
        //         abort();
        //     }
        // }

        buf_addr = hbwrapper::allocateBuffer(BUF_SIZE, 64);
        if (hbwrapper::registerMr(COMMON_PD, GET_MR_GLOBAL(pl_id, DIV_WRITER), buf_addr, BUF_SIZE) != 0) {
            abort();
        }

        buf_addr = hbwrapper::allocateBuffer(BUF_SIZE, 64);
        if (hbwrapper::registerMr(COMMON_PD, GET_MR_GLOBAL(pl_id, DIV_DEPCHECKER), buf_addr, BUF_SIZE) != 0) {
            abort();
        }

    }

    for (int pl_id = 0; pl_id < nplayers; pl_id++) {

        // for (int sp = 0; sp < arg_subpar; sp++) {
        //     if (pl_id != node_id) {
        //         hbwrapper::registerRcQp(
        //             COMMON_PD, 
        //             GET_QP_REPLICATOR(node_id, pl_id, sp),
        //             GET_SCQ_REPLICATOR(node_id, pl_id, sp),
        //             GET_RCQ_REPLICATOR(node_id, pl_id, sp)
        //             );
                
        //         hbwrapper::registerRcQp(
        //             COMMON_PD,
        //             GET_QP_REPLAYER(pl_id, sp),
        //             GET_SCQ_REPLAYER(pl_id, sp),
        //             GET_RCQ_REPLAYER(pl_id, sp)
        //         );
        //     }
        // }

        if (pl_id != node_id) {
            hbwrapper::registerRcQp(
                COMMON_PD, 
                GET_QP_REPLICATOR(node_id, pl_id, DIV_WRITER),
                GET_SCQ_REPLICATOR(node_id, pl_id, DIV_WRITER),
                GET_RCQ_REPLICATOR(node_id, pl_id, DIV_WRITER)
                );
            
            hbwrapper::registerRcQp(
                COMMON_PD,
                GET_QP_REPLAYER(pl_id, DIV_WRITER),
                GET_SCQ_REPLAYER(pl_id, DIV_WRITER),
                GET_RCQ_REPLAYER(pl_id, DIV_WRITER)
            );

            hbwrapper::registerRcQp(
                COMMON_PD, 
                GET_QP_REPLICATOR(node_id, pl_id, DIV_DEPCHECKER),
                GET_SCQ_REPLICATOR(node_id, pl_id, DIV_DEPCHECKER),
                GET_RCQ_REPLICATOR(node_id, pl_id, DIV_DEPCHECKER)
                );
            
            hbwrapper::registerRcQp(
                COMMON_PD,
                GET_QP_REPLAYER(pl_id, DIV_DEPCHECKER),
                GET_SCQ_REPLAYER(pl_id, DIV_DEPCHECKER),
                GET_RCQ_REPLAYER(pl_id, DIV_DEPCHECKER)
            );
        }
    }

    if (hbwrapper::exchangeRdmaConfigs() != 1) abort();

    for (int pl_id = 0; pl_id < nplayers; pl_id++) {
        // for (int sp = 0; sp < arg_subpar; sp++) {
            
        //     if (pl_id != node_id) {
        //         hbwrapper::connectRcQps(
        //             GET_QP_REPLICATOR(node_id, pl_id, sp), pl_id, COMMON_PD, GET_QP_REPLAYER(node_id, sp));
        //         hbwrapper::connectRcQps(
        //             GET_QP_REPLAYER(pl_id, sp), pl_id, COMMON_PD, GET_QP_REPLICATOR(pl_id, node_id, sp));
        //     }
        // }

        if (pl_id != node_id) {
            hbwrapper::connectRcQps(
                GET_QP_REPLICATOR(node_id, pl_id, DIV_WRITER), pl_id, COMMON_PD, GET_QP_REPLAYER(node_id, DIV_WRITER));
            hbwrapper::connectRcQps(
                GET_QP_REPLAYER(pl_id, DIV_WRITER), pl_id, COMMON_PD, GET_QP_REPLICATOR(pl_id, node_id, DIV_WRITER));

            hbwrapper::connectRcQps(
                GET_QP_REPLICATOR(node_id, pl_id, DIV_DEPCHECKER), pl_id, COMMON_PD, GET_QP_REPLAYER(node_id, DIV_DEPCHECKER));
            hbwrapper::connectRcQps(
                GET_QP_REPLAYER(pl_id, DIV_DEPCHECKER), pl_id, COMMON_PD, GET_QP_REPLICATOR(pl_id, node_id, DIV_DEPCHECKER));
        }
    }
}


soren::Connector::~Connector() {
    hbwrapper::cleanHartebeest();
}

int soren::Connector::getNodeId() {
    return node_id;
}

int soren::Connector::getNumPlayers() { return nplayers; }

void soren::prepareNextAlignedOffset(size_t& arg_offs, size_t& arg_free, size_t arg_tarsize) {
                            
    arg_tarsize += (sizeof(struct HeaderSlot) + sizeof(struct SlotCanary));

    // 1. Align, by 64.
    if ((arg_offs % ALIGNMENT) != 0) {
        arg_offs += (ALIGNMENT - (arg_offs % ALIGNMENT));
        arg_free -= arg_offs; 
    }
    
    // 2. Size remaining lack?
    if (arg_tarsize > (arg_free)) {
        arg_offs = 128;
        arg_free = BUF_SIZE - 128;
    }
}