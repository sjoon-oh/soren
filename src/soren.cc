/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <memory>
#include <atomic>

#include <unistd.h>

#include "soren.hh"
#include "hartebeest-wrapper.hh"

namespace soren {

    // Refer to https://github.com/sjoon-oh/hartebeest.
    // The Hartebeest project is not designed to have multiple instances that manages 
    //  the RDMA resource. Soren is built on the Hartebeest, thus it also should not 
    //  handle the resources by having multiple instances.
    //
    // A Connector literally connects to other nodes using RDMA. Refer to the file 
    //  connector.cc for its implementation.

    std::unique_ptr<Connector>  INST_CONNECTOR;     // Single global Connector instance.
    std::unique_ptr<Replayer>   INST_REPLAYER;      // Single global Replayer instance.
    std::unique_ptr<Replicator> INST_REPLICATOR;    // Single global Replicator instance.

    std::atomic<uint32_t>   glob_ranger;            // Globally manages the range of each hash values.
    std::atomic<uint32_t>   glob_subpar;            // Globally manages the sub partition.
                                                    // Curent version assumes that the sub partition 
                                                    // do not change dynamically in runtime.
    
    uint32_t                glob_node_id;           // What is this nodes ID?
    uint32_t                glob_nplayers;          // What is the number of players in a network?
}



/// @brief Initializes Soren.
/// @param arg_ranger 
/// @param arg_subpar 
void soren::initSoren(uint32_t arg_ranger, uint32_t arg_subpar) {

    INST_CONNECTOR.reset(new Connector(arg_subpar));    

    // Connector exchanges the network information automatically
    // in its contructor. 
    // If contstructed without any problem, you should be ready to 
    // interact with other nodes.

    glob_node_id   = INST_CONNECTOR->getNodeId();
    glob_nplayers  = hbwrapper::getNumPlayers();

    glob_ranger.store(arg_ranger);
    glob_subpar.store(arg_subpar);

    //
    // Initiate the replayer
    INST_REPLAYER.reset(new Replayer(glob_node_id, glob_nplayers, arg_ranger, arg_subpar));
    
    //
    // Initiate the replicator. If this node ID is 'some_id', 
    //  launch a replicator thread that distributes data in 'some_id' space.
    // Before initiating Replicator worker threads,
    //  make sure that all replayers in a same node are initiated.
    INST_REPLICATOR.reset(new Replicator(glob_node_id, glob_nplayers, arg_ranger, arg_subpar));

    //
    // There are 2*(n-1)*subpar replayers in Soren. 
    //  Replayers' objective is to inject replicated request into the applications,
    //  thus should have its own Memory Region. 
    // 
    // Note that all Memory Regions have thsir distinct IDs, based on its 
    // node ID and sub partitions.
    // For other node IDs, a machine should launch Replayers (receivers) to handle
    // the replications.
    //
    for (int nid = 0; nid < glob_nplayers; nid++) {
        for (int sp = 0; sp < arg_subpar; sp++) {
            if (nid != glob_node_id)            // For this node ID 'some_id', do not launch replayer,
                                                // since it is 'some_id's responsibility to let data copied.
                INST_REPLAYER->doAddLocalMr(
                    GET_MR_GLOBAL(nid, sp),     // Globally decided MR ID. Decided across the nodes.
                    soren::hbwrapper::getLocalMr(GET_MR_GLOBAL(nid, sp))
                );
        }
    }

    // Note that there is a single replication. 
    //  Each threads are managed by an instance. 
    //  Call doLaunchPlayer() to let a thread run.
    // 
    // A thread is written in lambda, thus refer to replayer.cc file.
    //
    for (int nid = 0; nid < glob_nplayers; nid++) {
        if (nid != glob_node_id) {
            for (int sp = 0; sp < arg_subpar; sp++)
                INST_REPLAYER->doLaunchPlayer(nid, sp, INST_REPLICATOR.get());
        }
    }

    //
    // Initiate the replicator. If this node ID is 'some_id', 
    //  launch a replicator thread that distributes data in 'some_id' space.
    // Before initiating Replicator worker threads,
    //  make sure that all replayers in a same node are initiated.
    // INST_REPLICATOR.reset(new Replicator(glob_node_id, glob_nplayers, arg_ranger, arg_subpar));

    //
    // A replicator (or its threads) is the one who do the RDMA writes/reads. 
    // Unlike the replicator who only needs Memory Region (for polling),
    // the replicator threads should have both MRs and QPs.
    for (int nid = 0; nid < glob_nplayers; nid++) {
        for (int sp = 0; sp < arg_subpar; sp++) {
            if (nid == glob_node_id) {                      // For this node ID 'some_id', 
                INST_REPLICATOR->doAddLocalMr(              // register a local MR.
                    GET_MR_GLOBAL(glob_node_id, sp),        // using globally decided MR ID, decided across the nodes.
                    soren::hbwrapper::getLocalMr(GET_MR_GLOBAL(glob_node_id, sp)));
            }

            if (nid != glob_node_id) { // Queue Pair for others to send.
                INST_REPLICATOR->doAddLocalQp(
                    GET_QP_REPLICATOR(glob_node_id, nid, sp), 
                    soren::hbwrapper::getLocalQp(GET_QP_REPLICATOR(glob_node_id, nid, sp)));
                
                // Now, the replicator should hold additional information such as 
                // remote's Memory Region RKEY, starting address, and size.
                // Without these information, any RDMA read or writes will fail. 
                //
                // The Connector module will have all RDMA information ready 
                // thanks to Hartebeest. 
                // doAddRemoteMr() inserts remote Replicator's Memory Region information
                // in ibv_mr struture. This is minimally filled. Only <rkey, addr, length>
                // is valid (if not local).
                //

                INST_REPLICATOR->doAddRemoteMr(             
                    REMOTE_REPLAYER_MR_2_LOCAL(GET_MR_GLOBAL(nid, sp), nid, sp), 
                    soren::hbwrapper::getRemoteMinimalMr(
                        nid, soren::COMMON_PD, GET_MR_GLOBAL(glob_node_id, sp)
                    )
                );
            }
        }
    }

    //
    // Launch the replicator threads. If this node ID is 'some_id', 
    for (int sp = 0; sp < arg_subpar; sp++)
        INST_REPLICATOR->doLaunchPlayer(glob_nplayers, sp);
}



/// @brief Do allocated resource clean ups.
void soren::cleanSoren() {

    // Kill all threads. 
    // This do the soft-landing of each thread, rather than forceful killing.
    for (int sp = 0; sp < soren::MAX_NWORKER; sp++) {
        INST_REPLAYER->doTerminateWorker(sp);
        INST_REPLICATOR->doTerminateWorker(sp);
    }

    sleep(10);      // Give some space. Will ya?

    //
    // Call dtors of instances.
    INST_CONNECTOR.release();
    INST_REPLAYER.release();
    INST_REPLICATOR.release();
}

soren::Connector*   soren::getConnector() { return INST_CONNECTOR.get(); }
soren::Replayer*    soren::getReplayer() { return INST_REPLAYER.get(); }
soren::Replicator*  soren::getReplicator() { return INST_REPLICATOR.get(); }