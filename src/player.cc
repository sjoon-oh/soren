/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

// #include <memory>

#include "logger.hh"
#include "commons.hh"
#include "player.hh"

#include "hartebeest-wrapper.hh"

namespace soren {
    static Logger PLAYER_LOGGER("SOREN/PLAYER", "soren_player.log");
}

//
// Player methods
void soren::Player::__sendWorkerSignal(uint32_t arg_hdl, int32_t arg_sig) {

    workers.at(arg_hdl).wrk_sig.store(arg_sig);

    if (arg_sig == SIG_SELFRET) {
        // Clean up
        workers.at(arg_hdl).wrk_sig.store(SIG_PAUSE);
        workers.at(arg_hdl).wrkt_nhdl = 0;
    }
}

void soren::Player::__waitWorkerSignal(uint32_t arg_hdl, int32_t arg_sig) {
    
    auto& signal = workers.at(arg_hdl).wrk_sig;
    while (signal.load() != arg_sig)
        ;
}

soren::Player::Player(uint16_t arg_players, uint32_t arg_ranger = 10, uint32_t arg_subpar = 1) 
    : sub_par(arg_subpar), judge(arg_players, arg_ranger) { }


bool soren::Player::doAddMr(uint32_t arg_id, struct ibv_mr* arg_mr) {

    if (mr_hdls.find(arg_id) != mr_hdls.end()) return false;
    mr_hdls.insert(
        std::pair<uint32_t, struct ibv_mr*>(arg_id, arg_mr));
    
    SOREN_LOGGER_INFO(PLAYER_LOGGER, "MR({}) handle added to the player.", arg_id);
    return true;
}

bool soren::Player::doAddQp(uint32_t arg_id, struct ibv_qp* arg_qp) {

    if (qp_hdls.find(arg_id) != qp_hdls.end()) return false;
    qp_hdls.insert(
        std::pair<uint32_t, struct ibv_qp*>(arg_id, arg_qp));

    SOREN_LOGGER_INFO(PLAYER_LOGGER, "QP({}) handle added to the player.", arg_id);
    return true;
}

void soren::Player::doResetMrs() { mr_hdls.clear(); }
void soren::Player::doResetQps() { qp_hdls.clear(); }

void soren::Player::doTerminateWorker(uint32_t arg_hdl) { __sendWorkerSignal(arg_hdl, SIG_SELFRET); }

void soren::Player::doForceKillWorker(uint32_t arg_hdl) {

    pthread_t native_handle = static_cast<pthread_t>(workers.at(arg_hdl).wrkt_nhdl);
    if (native_handle != 0)
        pthread_cancel(native_handle);

    // Reset all.
    workers.at(arg_hdl).wrkt_nhdl = 0;
    workers.at(arg_hdl).wrk_sig.store(0);
}

bool soren::Player::isWorkerAlive(uint32_t arg_hdl) {
    return (workers.at(arg_hdl).wrkt_nhdl != 0);
}


//
// Replicator methods
int soren::Replicator::__findEmptyWorkerHandle() {
    int hdl;
    for (hdl = 0; hdl < MAX_REPLICATOR_WORKER; hdl++) {
        if (workers[hdl].wrkt_nhdl == 0)
            return hdl;
    }

    if (hdl == MAX_NWORKER) return -1;
}

int soren::Replicator::__findNeighborAliveWorkerHandle(uint32_t arg_hdl) {
    
    int hdl = arg_hdl + 1;
    if (arg_hdl == MAX_REPLICATOR_WORKER) hdl = 0;

    for ( ; hdl < MAX_REPLICATOR_WORKER; hdl++)
        if (isWorkerAlive(hdl))
            return hdl;
    
    return -1;
}

soren::Replicator::Replicator(uint16_t arg_players, uint32_t arg_ranger = 10, uint32_t arg_subpar = 1) : 
    Player(arg_players, arg_ranger, arg_subpar) {
        if (arg_subpar > MAX_SUBPAR)
            sub_par = MAX_SUBPAR;
    }

int soren::Replicator::doLaunchPlayer() {

    // struct ::ibv_mr* mr = mr_hdls.find(arg_mr_id)->second;
    // struct ::ibv_qp* qp = qp_hdls.find(arg_qp_id)->second;

    int handle = __findEmptyWorkerHandle();

    WorkerThread& wrkr_inst = workers.at(handle);
    // auto& sig_var = wrkr_inst.wrk_sig;

    std::thread player_thread(
        [](
            std::atomic<int32_t>& arg_sig,
            const int arg_hdl,
            std::map<uint32_t, struct ::ibv_mr*>& arg_mr_hdls,
            std::map<uint32_t, struct ::ibv_qp*>& arg_qp_hdls) {
            
            Logger worker_logger("SOREN/PLAYER/WT", "soren_player_wt.log");

            uint32_t curr_log_idx = 0;
            uint32_t prev_log_idx = 0;

            bool disp_msg = false;

            int32_t signal = 0;

            while (1) {
                signal = arg_sig.load();

                switch (signal) {
                    case SIG_PAUSE:
                        if (disp_msg == false) {
                            SOREN_LOGGER_INFO(worker_logger, "Worker({}) pending...", arg_hdl);
                            disp_msg = true;
                        }
                        break;

                    case SIG_SELFRET:
                        SOREN_LOGGER_INFO(worker_logger, "Worker({}) terminated", arg_hdl);
                        return 0;

                    case SIG_PROPOSE:

                        // Something end.




                        arg_sig.store(SIG_WORKEND);
                        break;
                    default:
                        ;


                }
            }
            
            
                
            return 0;
        }, 
        std::ref(wrkr_inst.wrk_sig), handle, std::ref(mr_hdls), std::ref(qp_hdls)
    );

    wrkr_inst.wrkt        = std::move(player_thread);
    wrkr_inst.wrkt_nhdl   = wrkr_inst.wrkt.native_handle();

    SOREN_LOGGER_INFO(PLAYER_LOGGER, "Worker thread launched: handle({}), native handle({})", handle, wrkr_inst.wrkt_nhdl);

    wrkr_inst.wrkt.detach();
    
    return handle;
}

void soren::Replicator::doPropose(uint8_t* arg_addr, size_t arg_size, uint16_t arg_keypref) {

    // Here eveything will be replicated.
    // Sub partition can 8 max.
    uint32_t owner = 0;
    if (sub_par > 1) {
        owner = arg_keypref % sub_par;

        if (isWorkerAlive(owner))
            owner = __findNeighborAliveWorkerHandle(owner);
        
        if (owner == -1) {
            SOREN_LOGGER_INFO(PLAYER_LOGGER, "Unable to find alternative neighbor");
            return;
        }
    }
    else
        ;

    SOREN_LOGGER_INFO(PLAYER_LOGGER, "Worker({}) processing a PROPOSE.", owner);

    __sendWorkerSignal(owner, SIG_PROPOSE);
    __waitWorkerSignal(owner, SIG_WORKEND);

    SOREN_LOGGER_INFO(PLAYER_LOGGER, " > Work end signal from worker({}).", owner);
}




//
// Replayer methods