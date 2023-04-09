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

    soren::Replicator replicator(node_id, RANGER, SUB_PARTITON);

    for (int i = 0; i < num_players; i++) {

        if (i != node_id) {
            replicator.doAddMr(
                MRID(node_id, SUB_PARTITON),
                soren::hbwrapper::getMr(MRID(node_id, SUB_PARTITON))); // Test
            replicator.doAddQp(
                QPID_REPLICATOR(node_id, i, SUB_PARTITON)
                , soren::hbwrapper::getQp(QPID_REPLICATOR(node_id, i, SUB_PARTITON)));
        }
    }

    int worker_handle = replicator.doLaunchPlayer();

    SOREN_LOGGER_INFO(soren_lgr, "Replicator worker handle: {}", worker_handle);

    replicator.doPropose(nullptr, 0);
    replicator.doTerminateWorker(worker_handle);

    SOREN_LOGGER_INFO(soren_lgr, "Soren player test run end.");

    return 0;
}