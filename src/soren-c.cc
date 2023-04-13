/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <stdint.h>

#include "soren-c.h"

// #include "replayer.hh"
// #include "replicator.hh"
// #include "connector.hh"
#include "soren.hh"
#include "hartebeest-wrapper.hh"

#include <stdio.h>

uint32_t glob_node_id;

uint32_t glob_ranger;
uint32_t glob_nplayers;

void*   glob_connector;
void*   glob_replayer;
void*   glob_replicator;

void* cwInitConnection(uint32_t arg_subpar) {
    return reinterpret_cast<soren::Connector*>(new soren::Connector(arg_subpar));
}

void cwCleanConnection(void* arg_conn) {
    auto conn = reinterpret_cast<soren::Connector*>(arg_conn);
    delete conn;
}

int cwGetNumPlayers(void* arg_conn) {
    auto conn = reinterpret_cast<soren::Connector*>(arg_conn);
    return conn->getNumPlayers();
}

void cwInitSoren(uint32_t arg_ranger, uint32_t arg_subpar) {

    glob_connector = cwInitConnection(arg_subpar);

    glob_node_id = reinterpret_cast<soren::Connector*>(glob_connector)->getNodeId();
    glob_ranger = arg_ranger;
    glob_nplayers = cwGetNumPlayers(glob_connector);

    printf("Global Players: %d\n", glob_nplayers);

    glob_replayer = reinterpret_cast<soren::Replayer*>(
        new soren::Replayer(glob_node_id, glob_nplayers, glob_ranger, arg_subpar));

    for (int nid = 0; nid < glob_nplayers; nid++) {
        for (int sp = 0; sp < arg_subpar; sp++) {
            if (nid != glob_node_id) 
                reinterpret_cast<soren::Replayer*>(glob_replayer)->doAddLocalMr(
                    GET_MR_GLOBAL(nid, sp), 
                    soren::hbwrapper::getLocalMr(GET_MR_GLOBAL(nid, sp))
                );
        }
    }

    for (int nid = 0; nid < glob_nplayers; nid++) {
        if (nid != glob_node_id) {
            for (int sp = 0; sp < arg_subpar; sp++)
                reinterpret_cast<soren::Replayer*>(glob_replayer)->doLaunchPlayer(nid, sp);
        }
    }

    glob_replicator = reinterpret_cast<soren::Replicator*>(
        new soren::Replicator(glob_node_id, glob_nplayers, glob_ranger, arg_subpar));

    for (int nid = 0; nid < glob_nplayers; nid++) {
        for (int sp = 0; sp < arg_subpar; sp++) {
            if (nid == glob_node_id) {
                reinterpret_cast<soren::Replicator*>(glob_replicator)->doAddLocalMr(
                    GET_MR_GLOBAL(glob_node_id, sp),
                    soren::hbwrapper::getLocalMr(GET_MR_GLOBAL(glob_node_id, sp))); 
                        // Replicator's MR
            }

            if (nid != glob_node_id) { // Queue Pair for others to send.
                reinterpret_cast<soren::Replicator*>(glob_replicator)->doAddLocalQp(
                    GET_QP_REPLICATOR(glob_node_id, nid, sp), 
                    soren::hbwrapper::getLocalQp(GET_QP_REPLICATOR(glob_node_id, nid, sp)));

                reinterpret_cast<soren::Replicator*>(glob_replicator)->doAddRemoteMr(
                    GET_MR_GLOBAL(nid, sp), 
                    soren::hbwrapper::getRemoteMinimalMr(
                        nid, soren::COMMON_PD, GET_MR_GLOBAL(glob_node_id, sp)
                    )
                );
            }
        }
    }

    for (int sp = 0; sp < arg_subpar; sp++)
        reinterpret_cast<soren::Replicator*>(glob_replicator)->doLaunchPlayer(glob_nplayers, sp);
}

void cwPropose(uint8_t* arg_addr, size_t arg_size, uint16_t arg_keypref) {

    reinterpret_cast<soren::Replicator*>(glob_replicator)->doPropose(
        arg_addr, arg_size, arg_keypref
    );
}

void cwCleanSoren() {

    for (int hdl = 0; hdl < soren::MAX_NWORKER; hdl++) {
        reinterpret_cast<soren::Replicator*>(glob_replicator)->doTerminateWorker(hdl);
        reinterpret_cast<soren::Replayer*>(glob_replayer)->doTerminateWorker(hdl);
    }

    sleep(10);

    delete reinterpret_cast<soren::Replicator*>(glob_replicator);
    delete reinterpret_cast<soren::Replayer*>(glob_replayer);
    delete reinterpret_cast<soren::Connector*>(glob_connector);
}