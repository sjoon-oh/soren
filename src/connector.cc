/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "logger.hh"
#include "hartebeest-wrapper.hh"
#include "connector.hh"

#include "commons.hh"

namespace soren {

    static Logger CONNECTOR_LOGGER("SOREN/CONNECTOR", "soren_connector.log");
}

soren::Connector::Connector(uint32_t arg_subpar) {
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
        for (int sp = 0; sp < arg_subpar; sp++) {

            buf_addr = hbwrapper::allocateBuffer(BUF_SIZE, 64);
            if (hbwrapper::registerMr(COMMON_PD, MRID(pl_id, sp), buf_addr, BUF_SIZE) != 0) {
                SOREN_LOGGER_ERROR(CONNECTOR_LOGGER, "MR creation error.");
                abort();
            }
        }
    }

    for (int pl_id = 0; pl_id < nplayers; pl_id++) {

        for (int sp = 0; sp < arg_subpar; sp++) {
            if (pl_id != node_id) {
                hbwrapper::registerRcQp(
                    COMMON_PD, 
                    QPID_REPLICATOR(node_id, pl_id, sp),
                    SCQID_REPLICATOR(node_id, pl_id, sp),
                    RCQID_REPLICATOR(node_id, pl_id, sp)
                    );
                
                hbwrapper::registerRcQp(
                    COMMON_PD,
                    QPID_REPLAYER(pl_id, sp),
                    SCQID_REPLAYER(pl_id, sp),
                    RCQID_REPLAYER(pl_id, sp)
                );
            }
        }
    }

    if (hbwrapper::exchangeRdmaConfigs() != 1) abort();

    for (int pl_id = 0; pl_id < nplayers; pl_id++) {
        for (int sp = 0; sp < arg_subpar; sp++) {
            
            if (pl_id != node_id) {
                hbwrapper::connectRcQps(
                    QPID_REPLICATOR(node_id, pl_id, sp), pl_id, COMMON_PD, QPID_REPLAYER(node_id, sp));
                hbwrapper::connectRcQps(
                    QPID_REPLAYER(pl_id, sp), pl_id, COMMON_PD, QPID_REPLICATOR(pl_id, node_id, sp));
            }
        }
    }

    SOREN_LOGGER_INFO(CONNECTOR_LOGGER, "Connection setting end.");
}


soren::Connector::~Connector() {
    hbwrapper::cleanHartebeest();
}

int soren::Connector::getNodeId() {
    return node_id;
}

int soren::Connector::getNumPlayers() { return nplayers; }

void soren::prepareNextAlignedOffset(int& arg_offs, int& arg_free, int arg_tarsize) {
                            
    arg_tarsize += sizeof(struct Slot) + sizeof(struct SlotCanary);

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