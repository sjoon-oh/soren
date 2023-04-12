/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>

#include "../hartebeest-wrapper.hh"
#include "../soren.hh"

static soren::Logger soren_lgr("SOREN-CONN-TEST", "soren-connector.test.log");

int main() {

    SOREN_LOGGER_INFO(soren_lgr, "Soren connector test run.");

    soren::Connector conn;

    int node_id = conn.getNodeId();
    int num_players = soren::hbwrapper::getNumPlayers();

    SOREN_LOGGER_INFO(soren_lgr, "Soren connector test end.");
    SOREN_LOGGER_INFO(soren_lgr, "Soren player test run.");

    const int RANGER = 10;
    const int SUB_PARTITON = 1;

    soren::Replayer replayer(node_id, num_players, RANGER, SUB_PARTITON);

    for (int i = 0; i < num_players; i++) {
        for (int sp = 0; sp < SUB_PARTITON; sp++) {
            if (i != node_id) {
                
                replayer.doAddLocalMr(
                    GET_MR_GLOBAL(i, sp), soren::hbwrapper::getLocalMr(GET_MR_GLOBAL(i, sp))
                );
            }
        }
    }

    int worker_handle;
    for (int i = 0; i < num_players; i++) {
        if (i != node_id) {
            for (int sp = 0; sp < SUB_PARTITON; sp++) {
                replayer.doLaunchPlayer(i, sp);
            }
        }
    }

    soren::Replicator replicator(node_id, num_players, RANGER, SUB_PARTITON);

    for (int i = 0; i < num_players; i++) {
        for (int sp = 0; sp < SUB_PARTITON; sp++) {
            if (i == node_id) {
                replicator.doAddLocalMr(
                    GET_MR_GLOBAL(node_id, sp),
                    soren::hbwrapper::getLocalMr(GET_MR_GLOBAL(node_id, sp))); // Replicator's MR
            }

            if (i != node_id) { // Queue Pair for others to send.
                replicator.doAddLocalQp(
                    GET_QP_REPLICATOR(node_id, i, sp), 
                    soren::hbwrapper::getLocalQp(GET_QP_REPLICATOR(node_id, i, sp)));

                replicator.doAddRemoteMr(
                    GET_MR_GLOBAL(i, sp), 
                    soren::hbwrapper::getRemoteMinimalMr(
                        i, soren::COMMON_PD, GET_MR_GLOBAL(node_id, sp)
                    )
                );
            }
        }
    }

    worker_handle = replicator.doLaunchPlayer(num_players, 0);

    SOREN_LOGGER_INFO(soren_lgr, "Replicator worker handle: {}", worker_handle);

    // Sample buffer
    char* buf = "The past can hurt. But from the way I see it, you can either run from it, or learn from it.";

    for (int i = 0; i < 10; i++) {
        replicator.doPropose(reinterpret_cast<uint8_t*>(buf + (i * 6)), 6);
        sleep(0.5);
    }

    SOREN_LOGGER_INFO(soren_lgr, "Waiting for 20 seconds...");
    sleep(20);


    // replicator.doTerminateWorker(worker_handle);
    replayer.doTerminateWorker(0);
    replayer.doTerminateWorker(1);
    replicator.doTerminateWorker(worker_handle);

    SOREN_LOGGER_INFO(soren_lgr, "Soren player test run end.");


    return 0;
}