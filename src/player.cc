/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "logger.hh"
#include "player.hh"

namespace soren {
    static Logger PLAYER_LOGGER("SOREN/PLAYER", "soren_player.log");
}

//
// Player methods
soren::Player::Player(uint16_t arg_players, uint32_t arg_ranger = 10, uint32_t arg_subpar = 1) 
    : sub_par(arg_subpar), stop_and_go(WORKER_PAUSE), judge(arg_players, arg_ranger) { }

void soren::Player::doContinueAllWorkers() { stop_and_go.store(WORKER_CONTINUE); }
void soren::Player::doPauseAllWorkers() { stop_and_go.store(WORKER_PAUSE); }

bool soren::Player::doAddMr(uint32_t arg_id, struct ibv_mr* arg_mr) {

    if (mr_hdls.find(arg_id) != mr_hdls.end()) return false;
    mr_hdls.insert(
        std::pair<uint32_t, struct ibv_mr*>(arg_id, arg_mr));
    
    SOREN_LOGGER_INFO(PLAYER_LOGGER, "Added Mr to the player: {}", arg_id);
    return true;
}

bool soren::Player::doAddQp(uint32_t arg_id, struct ibv_qp* arg_qp) {

    if (qp_hdls.find(arg_id) != qp_hdls.end()) return false;
    qp_hdls.insert(
        std::pair<uint32_t, struct ibv_qp*>(arg_id, arg_qp));

    SOREN_LOGGER_INFO(PLAYER_LOGGER, "Added QP to the player: {}", arg_id);
    return true;
}

void soren::Player::doResetMrs() { mr_hdls.clear(); }
void soren::Player::doResetQps() { qp_hdls.clear(); }

void soren::Player::doKillWorker(uint32_t arg_hdl) {
    
    pthread_t native_handle = static_cast<pthread_t>(workers.at(arg_hdl).wrkt_nhdl);
    pthread_cancel(native_handle);

    workers.at(arg_hdl).wrkt_nhdl = 0;
}

bool soren::Player::isWorkerAlive(uint32_t arg_hdl) {
    return (workers.at(arg_hdl).wrkt_nhdl != 0);
}


//
// Replicator methods
soren::Replicator::Replicator(uint16_t arg_players, uint32_t arg_ranger = 10, uint32_t arg_subpar = 1) : 
    Player(arg_players, arg_ranger, arg_subpar) { }

int soren::Replicator::doLaunchPlayer(uint32_t arg_mr_id, uint32_t arg_qp_id) {

    if (mr_hdls.find(arg_mr_id) == mr_hdls.end()) return -1;
    if (qp_hdls.find(arg_mr_id) == qp_hdls.end()) return -2;

    struct ibv_mr* mr = mr_hdls.find(arg_mr_id)->second;
    struct ibv_qp* qp = qp_hdls.find(arg_qp_id)->second;

    int handle = workers.size();

    WorkerThread wrkr;                  // Default ctor.
    workers.push_back(std::move(WorkerThread()));

    // SOREN_LOGGER_INFO(PLAYER_LOGGER, "Worker added to the list: {}", workers.size());

    WorkerThread& wrkr_inst = workers.back();

    std::thread player_thread(
        [](std::atomic<bool>& arg_stop_and_go, 
            struct ibv_mr* arg_mr, struct ibv_qp* arg_qp) {
            
            Logger worker_logger("SOREN/PLAYER/WT", "soren_player_wt.log");

            struct ibv_mr loc_mr = *arg_mr;
            struct ibv_qp loc_qp = *arg_qp;

            uint32_t curr_log_idx = 0;
            uint32_t prev_log_idx = 0;

            bool pause_msg = false;
            bool contd_msg = false;
            
            while (1) {

                while(arg_stop_and_go.load() == WORKER_PAUSE) {
                    if (pause_msg == false) {
                        SOREN_LOGGER_INFO(worker_logger, "Worker paused.");
                        pause_msg = true;
                        contd_msg = false;
                    }
                } // Pause consuming.

                if (contd_msg == false) {
                    SOREN_LOGGER_INFO(worker_logger, "Worker cont'd.");
                    pause_msg = false;
                    contd_msg = true;
                }
                
                // Do something here.









                
            }
        }, 
        std::ref(this->stop_and_go), 
        mr, qp
    );

    wrkr_inst.wrkt        = std::move(player_thread);
    wrkr_inst.wrkt_nhdl   = wrkr_inst.wrkt.native_handle();

    SOREN_LOGGER_INFO(PLAYER_LOGGER, "Worker thread launched: native handle({})", wrkr_inst.wrkt_nhdl);

    wrkr_inst.wrkt.detach();
    
    return handle;
}







//
// Replayer methods