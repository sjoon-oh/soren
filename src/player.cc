/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

// #include <memory>
#include <array>
#include <iostream>

#include "logger.hh"
#include "player.hh"

#include "connector.hh"
#include "hartebeest-wrapper.hh"

#include <infiniband/verbs.h>



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

soren::Player::Player(uint32_t arg_nid, uint16_t arg_players, uint32_t arg_ranger = 10, uint32_t arg_subpar = 1) 
    : node_id(arg_nid), sub_par(arg_subpar) { }


bool soren::Player::doAddLocalMr(uint32_t arg_id, struct ibv_mr* arg_mr) {

    if (mr_hdls.find(arg_id) != mr_hdls.end()) return false;
    mr_hdls.insert(
        std::pair<uint32_t, struct ibv_mr*>(arg_id, arg_mr));
    
    SOREN_LOGGER_INFO(PLAYER_LOGGER, "Handle MR({})({}): addr({}) added.", 
        arg_id, reinterpret_cast<void*>(arg_mr), arg_mr->addr);

    return true;
}

bool soren::Player::doAddLocalQp(uint32_t arg_id, struct ibv_qp* arg_qp) {

    if (qp_hdls.find(arg_id) != qp_hdls.end()) return false;
    qp_hdls.insert(
        std::pair<uint32_t, struct ibv_qp*>(arg_id, arg_qp));

    SOREN_LOGGER_INFO(PLAYER_LOGGER, "QP({}) handle added.", arg_id);
    return true;
}

bool soren::Player::doAddRemoteMr(uint32_t arg_id, struct ibv_mr* arg_mr) {
    
    doAddLocalMr(arg_id, arg_mr);

    mr_remote_hdls.push_back(arg_mr);
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

soren::Replicator::Replicator(uint32_t arg_nid, uint16_t arg_players, uint32_t arg_ranger = 10, uint32_t arg_subpar = 1) : 
    Player(arg_nid, arg_players, arg_ranger, arg_subpar) {
        if (arg_subpar > MAX_SUBPAR)
            sub_par = MAX_SUBPAR;
    }

int soren::Replicator::doLaunchPlayer(uint32_t arg_nplayers) {

    int handle = __findEmptyWorkerHandle();
    WorkerThread& wrkr_inst = workers.at(handle);

    //
    // Initiate a worker thread here.
    std::thread player_thread(
        [](
            std::atomic<int32_t>& arg_sig,                      // Signal
            std::atomic<int32_t>& arg_ws_free_idx,
            Slot* arg_ws,
            const int arg_hdl,                                  // Worker Handle
            
            // Local handles.
            std::map<uint32_t, struct ibv_mr*>& arg_mr_hdls,  // MR handle, for reference once.
            std::map<uint32_t, struct ibv_qp*>& arg_qp_hdls,  // QP handle, for reference once.
            
            const uint32_t arg_nid,                             // Node ID
            const uint32_t arg_npl,

            std::atomic<uint32_t>& arg_subpar                   // Sub partition, (in case config changes.)
        ) {
            
            Logger worker_logger("SOREN/REPLICATOR/W", "soren_player_wt.log");

            //
            // Local Memory Region (Buffer) tracker.
            int32_t     mr_offset = 0;
            int32_t     mr_linfree = BUF_SIZE;

            //
            // Prepare resources for RDMA operations.
            uint32_t wrkr_mr_id = MRID(arg_nid, arg_hdl);
            struct ibv_mr* wrkr_mr = arg_mr_hdls.find(wrkr_mr_id)->second;
            struct ibv_qp** qps = new struct ibv_qp*[arg_npl]; 
            struct ibv_mr** mrs = new struct ibv_mr*[arg_npl]; // Holds remote mrs.

            SOREN_LOGGER_INFO(worker_logger, "Worker({}) initiated.", arg_hdl);

            for (int nid = 0; nid < arg_npl; nid++) {
                for (int sp = 0; sp < arg_subpar.load(); sp++)
                    if (nid == arg_nid) {
                        qps[nid] = nullptr;
                        mrs[nid] = nullptr;
                    } else {
                        qps[nid] = arg_qp_hdls.find(QPID_REPLICATOR(arg_nid, nid, sp))->second;
                        mrs[nid] = arg_mr_hdls.find(MRID(nid, sp))->second;
                    }
            }

            // 
            // Worker handle also represents corresponding sub-partition. 
            // Thus, be sure not to kill any workers arbirarily.
            bool disp_msg = false;
            int32_t signal = 0, rnd_canary = 0;

            while (1) {

                signal = arg_sig.load();

                std::srand(std::time(nullptr));
                rnd_canary = std::rand();

                switch (signal) {
                    case SIG_PAUSE: 
                        if (disp_msg == false) {
                            SOREN_LOGGER_INFO(worker_logger, "Worker({}) pending...", arg_hdl);
                            disp_msg = true;
                        }
                        break;

                    case SIG_SELFRET:

                        delete[] qps;
                        delete[] mrs;

                        SOREN_LOGGER_INFO(worker_logger, "Worker({}) terminated", arg_hdl);
                        return 0;

                    case SIG_PROPOSE:
                        disp_msg = false;
                        SOREN_LOGGER_INFO(worker_logger, "Worker({}) PROPOSING...", arg_hdl);
                        
                        {   
                            int target_idx = arg_ws_free_idx.fetch_add(1);
                            
                            Slot slot = arg_ws[target_idx];
                            SlotCanary slot_canary = { .canary = rnd_canary };

                            slot.canary = rnd_canary;

                            //
                            // Make payload!
                            // 1. Alignment
                            prepareNextAlignedOffset(mr_offset, mr_linfree, slot.size);
                            SOREN_LOGGER_INFO(worker_logger, " > offset: {}", mr_offset);

                            // 2. Fetch to the local memory region.
                            std::memcpy(
                                wrkr_mr->addr + mr_offset,        // Destination
                                reinterpret_cast<void*>(slot.addr), // Source
                                slot.size                           // Size, dah.
                            );

                            std::cout << (char*)(wrkr_mr->addr + mr_offset) << std::endl;

                            // 
                            // Send POST
                            for (int nid = 0; nid < arg_npl; nid++) {
                                if (nid == arg_nid) continue;
                                
                                // Update to remote contents.
                                slot.addr = mr_offset + reinterpret_cast<uintptr_t>(mrs[nid]->addr);

                                if (rdmaPost(
                                        IBV_WC_RDMA_WRITE, 
                                        qps[nid],               // Local Replicator's Queue Pair
                                        reinterpret_cast<uintptr_t>(wrkr_mr->addr) + mr_offset,
                                                                // Local buffer address
                                        slot.size,              // Buffer size
                                        wrkr_mr->lkey,          // Local MR LKey
                                        slot.addr,              // Remote's address
                                        mrs[nid]->rkey          // Remotes RKey
                                        )
                                    != 0)
                                    SOREN_LOGGER_ERROR(worker_logger, "");
                            }

                            mr_offset += slot.size;
                            mr_linfree = BUF_SIZE - mr_offset;
                        }
                        
                        arg_sig.store(SIG_WORKEND);
                        break;

                    default:
                        ;
                }
            }          
                
            return 0;
        }, 
            std::ref(wrkr_inst.wrk_sig), 
            std::ref(wrkr_inst.ws_free_idx), wrkr_inst.wrkspace, handle,
            std::ref(mr_hdls), std::ref(qp_hdls),
            node_id, arg_nplayers, std::ref(sub_par)
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
    uint32_t owner_hdl = 0;
    if (sub_par > 1) {
        owner_hdl = arg_keypref % sub_par;

        if (isWorkerAlive(owner_hdl))
            owner_hdl = __findNeighborAliveWorkerHandle(owner_hdl);
        
        if (owner_hdl == -1) {
            SOREN_LOGGER_INFO(PLAYER_LOGGER, "Unable to find alternative neighbor");
            return;
        }
    }
    else
        ;

    SOREN_LOGGER_INFO(PLAYER_LOGGER, "Prepare for PROPOSE...", owner_hdl);

    //
    // Working with slots
    Slot* workspace = workers.at(owner_hdl).wrkspace;
    int slot_idx = workers.at(owner_hdl).ws_free_idx.load();

    SOREN_LOGGER_INFO(PLAYER_LOGGER, "Sending target({}), size: {}, slot: {}", reinterpret_cast<void*>(arg_addr), arg_size, slot_idx);

    if (slot_idx == MAX_NSLOTS) {
        workers.at(owner_hdl).ws_free_idx.store(0);
        slot_idx = 0;
    }

    // Set the local buffer, and let the worker handle the slot.
    workspace[slot_idx].addr = reinterpret_cast<uintptr_t>(arg_addr);
    workspace[slot_idx].size = arg_size;

    __sendWorkerSignal(owner_hdl, SIG_PROPOSE);     // Let it replicate.
    __waitWorkerSignal(owner_hdl, SIG_WORKEND);

    SOREN_LOGGER_INFO(PLAYER_LOGGER, "Work END signal from worker({}).", owner_hdl);
}




//
// Replayer methods