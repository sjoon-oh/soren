/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <stdint.h>
#include <unistd.h>

#include "soren-c.h"
#include "soren.hh"
#include "hartebeest-wrapper.hh"

#include <stdio.h>

//
// This is C-Wrapper for functions in the file, soren.cc.

uint32_t glob_node_id;      // What is this nodes ID?

uint32_t glob_ranger;       // Globally manages the range of each hash values.
uint32_t glob_nplayers;     // What is the number of players in a network?

void*   glob_connector;     // Single global Connector instance.
void*   glob_replayer;      // Single global Replayer instance.
void*   glob_replicator;    // Single global Replicator instance.



/// @brief Allocates Connector instance at glob_connector.
/// @param arg_subpar 
/// @return 
void* cwInitConnection(uint32_t arg_subpar) {
    return reinterpret_cast<soren::Connector*>(new soren::Connector(arg_subpar));
}



/// @brief Clean Connector resource.
/// @param arg_conn 
void cwCleanConnection(void* arg_conn) {
    auto conn = reinterpret_cast<soren::Connector*>(arg_conn);
    delete conn;
}

int cwGetNumPlayers(void* arg_conn) {
    auto conn = reinterpret_cast<soren::Connector*>(arg_conn);
    return conn->getNumPlayers();
}



/// @brief Initialize Soren. This wraps components of each instance of Soren.
/// @param arg_ranger 
/// @param arg_subpar 
void cwInitSoren(uint32_t arg_ranger, uint32_t arg_subpar) {

    glob_connector = cwInitConnection(arg_subpar);

    // Connector exchanges the network information automatically
    // in its contructor. 
    // If contstructed without any problem, you should be ready to 
    // interact with other nodes.

    glob_node_id    = reinterpret_cast<soren::Connector*>(glob_connector)->getNodeId();
    glob_ranger     = arg_ranger;
    glob_nplayers   = cwGetNumPlayers(glob_connector);

    //
    // Initiate the replayer
    glob_replayer = reinterpret_cast<soren::Replayer*>(
        new soren::Replayer(glob_node_id, glob_nplayers, glob_ranger, arg_subpar));
    
    //
    // Initiate the replicator. If this node ID is 'some_id', 
    //  launch a replicator thread that distributes data in 'some_id' space.
    // Before initiating Replicator worker threads,
    //  make sure that all replayers in a same node are initiated.
    glob_replicator = reinterpret_cast<soren::Replicator*>(
        new soren::Replicator(glob_node_id, glob_nplayers, glob_ranger, arg_subpar));

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
                reinterpret_cast<soren::Replayer*>(glob_replayer)->doAddLocalMr(
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
                reinterpret_cast<soren::Replayer*>(glob_replayer)->doLaunchPlayer(nid, sp, reinterpret_cast<soren::Replicator*>(glob_replicator));
        }
    }

    //
    // Initiate the replicator. If this node ID is 'some_id', 
    //  launch a replicator thread that distributes data in 'some_id' space.
    // Before initiating Replicator worker threads,
    //  make sure that all replayers in a same node are initiated.
    // glob_replicator = reinterpret_cast<soren::Replicator*>(
    //     new soren::Replicator(glob_node_id, glob_nplayers, glob_ranger, arg_subpar));

    //
    // A replicator (or its threads) is the one who do the RDMA writes/reads. 
    // Unlike the replicator who only needs Memory Region (for polling),
    // the replicator threads should have both MRs and QPs.
    for (int nid = 0; nid < glob_nplayers; nid++) {
        for (int sp = 0; sp < arg_subpar; sp++) {
            if (nid == glob_node_id) {                      // For this node ID 'some_id', 
                                                            // register a local MR.
                reinterpret_cast<soren::Replicator*>(glob_replicator)->doAddLocalMr(    
                    GET_MR_GLOBAL(glob_node_id, sp),        // using globally decided MR ID, decided across the nodes.
                    soren::hbwrapper::getLocalMr(GET_MR_GLOBAL(glob_node_id, sp))); 
                        // Replicator's MR
            }

            if (nid != glob_node_id) { // Queue Pair for others to send.
                reinterpret_cast<soren::Replicator*>(glob_replicator)->doAddLocalQp(
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

                reinterpret_cast<soren::Replicator*>(glob_replicator)->doAddRemoteMr(
                    GET_MR_GLOBAL(nid, sp), 
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
        reinterpret_cast<soren::Replicator*>(glob_replicator)->doLaunchPlayer(glob_nplayers, sp);
}

void cwPropose(uint8_t* arg_memaddr, size_t arg_memsz, uint8_t* arg_keypref, size_t arg_keysz) {

    reinterpret_cast<soren::Replicator*>(glob_replicator)->doPropose(
        arg_memaddr, arg_memsz, arg_keypref, arg_keysz
    );
}



/// @brief Do allocated resource clean ups.
void cwCleanSoren() {

    // Kill all threads. 
    // This do the soft-landing of each thread, rather than forceful killing.
    for (int hdl = 0; hdl < soren::MAX_NWORKER; hdl++) {
        reinterpret_cast<soren::Replicator*>(glob_replicator)->doTerminateWorker(hdl);
        reinterpret_cast<soren::Replayer*>(glob_replayer)->doTerminateWorker(hdl);
    }

    sleep(10);      // Give some space. Will ya?
    printf("Workers softlanded.\n");

    delete reinterpret_cast<soren::Replicator*>(glob_replicator);
    delete reinterpret_cast<soren::Replayer*>(glob_replayer);
    delete reinterpret_cast<soren::Connector*>(glob_connector);
}

void cwInitTs(int32_t arg_nts) { soren::initTimestamps(arg_nts); }
void cwDumpTs() { soren::dumpElapsedTimes(); }

int32_t cwMarkTsBefore() { return soren::__MARK_TS_BEFORE__(); }
void cwMarkTsAfter(int32_t arg_idx) { soren::__MARK_TS_AFTER__(arg_idx); }

