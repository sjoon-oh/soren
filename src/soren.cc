/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <memory>
#include <atomic>

#include "soren.hh"
#include "hartebeest-wrapper.hh"

namespace soren {

    std::unique_ptr<Connector>  INST_CONNECTOR;
    std::unique_ptr<Replayer>   INST_REPLAYER;
    std::unique_ptr<Replicator> INST_REPLICATOR;

    std::atomic<uint32_t>   glob_ranger;
    std::atomic<uint32_t>   glob_subpar;

    uint32_t                glob_node_id;
    uint32_t                glob_nplayers;
}


void soren::initSoren(uint32_t arg_ranger, uint32_t arg_subpar) {

    INST_CONNECTOR.reset(new Connector(arg_subpar));

    glob_node_id   = INST_CONNECTOR->getNodeId();
    glob_nplayers  = hbwrapper::getNumPlayers();

    glob_ranger.store(arg_ranger);
    glob_subpar.store(arg_subpar);

    //
    // Initiate the replayer
    INST_REPLAYER.reset(new Replayer(glob_node_id, glob_nplayers, arg_ranger, arg_subpar));

    for (int nid = 0; nid < glob_nplayers; nid++) {
        for (int sp = 0; sp < arg_subpar; sp++) {
            if (nid != glob_node_id) 
                INST_REPLAYER->doAddLocalMr(
                    GET_MR_GLOBAL(nid, sp), soren::hbwrapper::getLocalMr(GET_MR_GLOBAL(nid, sp))
                );
        }
    }

    for (int nid = 0; nid < glob_nplayers; nid++) {
        if (nid != glob_node_id) {
            for (int sp = 0; sp < arg_subpar; sp++)
                INST_REPLAYER->doLaunchPlayer(nid, sp);
        }
    }

    //
    // Initiate the replicator
    INST_REPLICATOR.reset(new Replicator(glob_node_id, glob_nplayers, arg_ranger, arg_subpar));


    for (int nid = 0; nid < glob_nplayers; nid++) {
        for (int sp = 0; sp < arg_subpar; sp++) {
            if (nid == glob_node_id) {
                INST_REPLICATOR->doAddLocalMr(
                    GET_MR_GLOBAL(glob_node_id, sp),
                    soren::hbwrapper::getLocalMr(GET_MR_GLOBAL(glob_node_id, sp))); // Replicator's MR
            }

            if (nid != glob_node_id) { // Queue Pair for others to send.
                INST_REPLICATOR->doAddLocalQp(
                    GET_QP_REPLICATOR(glob_node_id, nid, sp), 
                    soren::hbwrapper::getLocalQp(GET_QP_REPLICATOR(glob_node_id, nid, sp)));

                INST_REPLICATOR->doAddRemoteMr(
                    GET_MR_GLOBAL(nid, sp), 
                    soren::hbwrapper::getRemoteMinimalMr(
                        nid, soren::COMMON_PD, GET_MR_GLOBAL(glob_node_id, sp)
                    )
                );
            }
        }
    }

    for (int sp = 0; sp < arg_subpar; sp++)
        INST_REPLICATOR->doLaunchPlayer(glob_nplayers, sp);
}

void soren::cleanSoren() {

    for (int sp = 0; sp < soren::MAX_NWORKER; sp++) {
        INST_REPLAYER->doTerminateWorker(sp);
        INST_REPLICATOR->doTerminateWorker(sp);
    }

    sleep(10);

    INST_CONNECTOR.release();
    INST_REPLAYER.release();
    INST_REPLICATOR.release();
}

soren::Connector* soren::getConnector() { return INST_CONNECTOR.get(); }
soren::Replayer* soren::getReplayer() { return INST_REPLAYER.get(); }
soren::Replicator* soren::getReplicator() { return INST_REPLICATOR.get(); }