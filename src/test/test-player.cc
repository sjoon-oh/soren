/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "../soren.hh"
#include "../hartebeest-wrapper.hh"

static soren::Logger soren_lgr("SOREN-CONN-TEST", "soren-connector.test.log");


int main() {

    const int this_node_id = soren::hbwrapper::getThisNodeId();
    const int RANGER = 10;
    const int NUM_PLAYERS = soren::hbwrapper::getNumPlayers();
    const int SUB_PARTITON = 1;

    soren::Replicator replicator(this_node_id, RANGER, SUB_PARTITON);

    

    int worker_handle = replicator.doLaunchPlayer(1, 1); // MR_ID, QP_ID
    SOREN_LOGGER_INFO(soren_lgr, "Replicator worker handle: {}", worker_handle);

    sleep(3);
    SOREN_LOGGER_INFO(soren_lgr, "Let all workers run.");
    replicator.doContinueAllWorkers();

    return 0;
}