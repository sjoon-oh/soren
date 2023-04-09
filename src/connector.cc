/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "logger.hh"
#include "hartebeest-wrapper.hh"
#include "connector.hh"

namespace soren {
    static Logger CONNECTOR_LOGGER("SOREN/CONNECTOR", "soren_connector.log");
}

soren::Connector::Connector(uint32_t arg_subpar) {
    hbwrapper::initHartebeest();
    hbwrapper::initRdmaConfigurator();
    hbwrapper::initConfigFileExchanger();

    int num_players = hbwrapper::getNumPlayers();
    node_id = hbwrapper::getThisNodeId();

    soren::hbwrapper::registerPd(COMMON_PD);
    
    //
    // Create Mr Qps for Receiver: Replayer.
    const size_t buf_size = 2 * 1024 * 1024;
    uint8_t* buf_addr = nullptr;

    for (int pl_id = 0; pl_id < num_players; pl_id++) {
        for (int sp = 0; sp < arg_subpar; sp++) {

            buf_addr = hbwrapper::allocateBuffer(buf_size, 64);
            hbwrapper::registerMr(COMMON_PD, MRID(pl_id, sp), buf_addr, buf_size);
        }
    }

    for (int pl_id = 0; pl_id < num_players; pl_id++) {

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

    hbwrapper::exchangeRdmaConfigs();

    for (int pl_id = 0; pl_id < num_players; pl_id++) {
        for (int sp = 0; sp < arg_subpar; sp++) {
            
            if (pl_id != node_id) {
                hbwrapper::connectRcQps(
                    QPID_REPLICATOR(node_id, pl_id, sp), pl_id, COMMON_PD, QPID_REPLAYER(pl_id, sp));
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