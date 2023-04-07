/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>

#include "../hartebeest-wrapper.hh"
#include "../soren.hh"

static soren::Logger soren_lgr("SOREN-CONN-TEST", "soren-connector.test.log");

void player_test() {

    const int this_node_id = soren::hbwrapper::getThisNodeId();
    const int RANGER = 10;
    const int NUM_PLAYERS = soren::hbwrapper::getNumPlayers();
    const int SUB_PARTITON = 1;

    soren::Replicator replicator(this_node_id, RANGER, SUB_PARTITON);

    replicator.doAddMr(0, soren::hbwrapper::getMr(1)); // Test
    replicator.doAddQp(0, soren::hbwrapper::getQp(1234));

    int worker_handle = replicator.doLaunchPlayer(0, 0); // MR_ID, QP_ID
    SOREN_LOGGER_INFO(soren_lgr, "Replicator worker handle: {}", worker_handle);

    sleep(3);
    SOREN_LOGGER_INFO(soren_lgr, "Let all workers run.");
    replicator.doContinueAllWorkers();

    sleep(3);
    SOREN_LOGGER_INFO(soren_lgr, "Let all workers rest.");
    replicator.doPauseAllWorkers();

    sleep(3);
    SOREN_LOGGER_INFO(soren_lgr, "Let all workers run.");
    replicator.doContinueAllWorkers();

    SOREN_LOGGER_INFO(soren_lgr, "Killing all workers.");
    replicator.doKillWorker(worker_handle);
}

int main() {

    // Test locals
    const int COMMON_PROTECTION_DOMAIN = 0;
    const size_t MEMORY_REGION_SIZE = 1024 * 1024;

    int this_node_id = 0;

    SOREN_LOGGER_INFO(soren_lgr, "Soren connector test run.");

    SOREN_LOGGER_INFO(soren_lgr, "Hartebeest initialization first run.");
    soren::hbwrapper::initHartebeest();

    SOREN_LOGGER_INFO(soren_lgr, "Hartebeest initialization second run.");
    soren::hbwrapper::initHartebeest();

    soren::hbwrapper::initRdmaConfigurator();


    // Protection Domain Register
    soren::hbwrapper::registerPd(COMMON_PROTECTION_DOMAIN);
    
    // Buffer Allocation 
    uint8_t* mr_addr = soren::hbwrapper::allocateBuffer(MEMORY_REGION_SIZE, 64);

    soren::hbwrapper::registerMr(
        COMMON_PROTECTION_DOMAIN,
        1, // MR ID
        mr_addr,
        MEMORY_REGION_SIZE
    );

    soren::hbwrapper::registerRcQp(
        COMMON_PROTECTION_DOMAIN, 1234, 12, 34
    );

    if (!soren::hbwrapper::initConfigFileExchanger())
        goto clean_up;

    soren::hbwrapper::exchangeRdmaConfigs();

    SOREN_LOGGER_INFO(soren_lgr, "Soren connector test end.");
    SOREN_LOGGER_INFO(soren_lgr, "Soren player test run.");

    player_test();

    SOREN_LOGGER_INFO(soren_lgr, "Soren player test run end.");

clean_up:
    soren::hbwrapper::cleanHartebeest();

    return 0;
}