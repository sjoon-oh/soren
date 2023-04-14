/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "logger.hh"
#include "replicator.hh"

#include "commons.hh"
#include "connector.hh"
#include "hartebeest-wrapper.hh"

#include <vector>
#include <infiniband/verbs.h>

#include <iostream>

//
// Replicator methods
namespace soren {
    static Logger REPLICATOR_LOGGER("SOREN/REPLICATOR", "soren_replicator.log");
}



/// @brief Sends a worker a signal.
/// @param arg_hdl 
/// @param arg_sig 
void soren::Replicator::__sendWorkerSignal(uint32_t arg_hdl, int32_t arg_sig) {
    workers.at(arg_hdl).wrk_sig.store(arg_sig);

    if (arg_sig == SIG_SELFRET)
        workers.at(arg_hdl).wrkt_nhdl = 0;
}



/// @brief Waits until a worker sets the signal.
/// @param arg_hdl 
/// @param arg_sig 
void soren::Replicator::__waitWorkerSignal(uint32_t arg_hdl, int32_t arg_sig) {
    
    auto& signal = workers.at(arg_hdl).wrk_sig;
    while (signal.load() != arg_sig)
        ;
}



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



/// @brief Constructor of Replicator instance.
/// @param arg_nid 
/// @param arg_players 
/// @param arg_ranger 
/// @param arg_subpar 
soren::Replicator::Replicator(uint32_t arg_nid, uint16_t arg_players, uint32_t arg_ranger = 10, uint32_t arg_subpar = 1) : 
    node_id(arg_nid), nplayers(arg_players), ranger(arg_ranger), sub_par(arg_subpar) {
        if (arg_subpar > MAX_SUBPAR)
            sub_par = MAX_SUBPAR;
    }

soren::Replicator::~Replicator() { for (auto& elem: mr_remote_hdls) delete elem; }



/// @brief doAddLocalMr inserts the allocated RDMA MR resource (on this machine).
/// @param arg_id 
/// @param arg_mr 
/// @return 
bool soren::Replicator::doAddLocalMr(uint32_t arg_id, struct ibv_mr* arg_mr) {

    if (mr_hdls.find(arg_id) != mr_hdls.end()) return false;
    mr_hdls.insert(
        std::pair<uint32_t, struct ibv_mr*>(arg_id, arg_mr));

    //
    // Since a worker thread should have its region to work with, 
    //  an MR generated from the Connector instance should be registered here.
    //  All Memory Regions should be registered for each worker thread spawned.
    // Note that the resources are managed by the connector. 
    //  Replicator just borrows it by having its pointer, but should never free.
    
    SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "MR({})[{}] => MR HDL map", 
        arg_id, reinterpret_cast<void*>(arg_mr), arg_mr->addr);

    return true;
}



/// @brief doAddLocalQp inerts the allocated RDMA QP resource (on this machine).
/// @param arg_id 
/// @param arg_qp 
/// @return 
bool soren::Replicator::doAddLocalQp(uint32_t arg_id, struct ibv_qp* arg_qp) {

    if (qp_hdls.find(arg_id) != qp_hdls.end()) return false;
    qp_hdls.insert(
        std::pair<uint32_t, struct ibv_qp*>(arg_id, arg_qp));

    //  A worker thread RDMA writes data to remotes. Thus, an QP generated from the 
    //  Connector instance should be registered here. Queue pair per thread should be 
    //  registered.
    // Note that the resources are managed by the connector. 
    //  Replicator just borrows it by having its pointer, but should never free.
    
    SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "QP({})[{}] => QP HDL map", arg_id, reinterpret_cast<void*>(arg_qp));
    return true;
}



/// @brief doAddLocalQp inerts the allocated RDMA QP resource of other machine.
/// @param arg_id 
/// @param arg_mr 
/// @return 
bool soren::Replicator::doAddRemoteMr(uint32_t arg_id, struct ibv_mr* arg_mr) {
    
    doAddLocalMr(arg_id, arg_mr);

    // A worker thread should have knowledge about the remote's Memory Region.
    // Without the information such as RKEY, starting address and its length, 
    // any RDMA reads or writes will never happen.
    // These information is exchanged thanks to Hartebeest. The argument arg_mr
    // holds only minimal elements that are necessary for communication. Everything
    // else is set to zero. 
    // Refer to searchQp for more info.

    mr_remote_hdls.push_back(arg_mr);
    return true;
}



void soren::Replicator::doResetMrs() { mr_hdls.clear(); }
void soren::Replicator::doResetQps() { qp_hdls.clear(); }

void soren::Replicator::doTerminateWorker(uint32_t arg_hdl) { __sendWorkerSignal(arg_hdl, SIG_SELFRET); }
void soren::Replicator::doForceKillWorker(uint32_t arg_hdl) {

    pthread_t native_handle = static_cast<pthread_t>(workers.at(arg_hdl).wrkt_nhdl);
    if (native_handle != 0)
        pthread_cancel(native_handle);

    // Reset all.
    workers.at(arg_hdl).wrkt_nhdl = 0;
    workers.at(arg_hdl).wrk_sig.store(0);
}




bool soren::Replicator::isWorkerAlive(uint32_t arg_hdl) {
    return (workers.at(arg_hdl).wrkt_nhdl != 0);
}



/// @brief Launches a worker thread, which handles arg_cur_sp sub partition.
/// @param arg_nplayers 
/// @param arg_cur_sp 
/// @return 
int soren::Replicator::doLaunchPlayer(uint32_t arg_nplayers, int arg_cur_sp) {

    int handle = __findEmptyWorkerHandle();         // Find the next index with unused WorkerThread array.
    WorkerThread& wrkr_inst = workers.at(handle);

    //
    // Initiate a worker thread here.
    std::thread player_thread(
        [](
            std::atomic<int32_t>& arg_sig,                      // Signal
            std::atomic<u_char>& arg_next_free_sidx,            // Points to the next index to process.
            std::atomic<u_char>& arg_finn_proc_sidx,            // Points to finished index.

            Slot* arg_slots,                                    // Workspace for this worker thread.
            const int arg_hdl,                                  // Worker Handle
            
            // Local handles.
            std::map<uint32_t, struct ibv_mr*>& arg_mr_hdls,    // MR handle, for referencing once.
            std::map<uint32_t, struct ibv_qp*>& arg_qp_hdls,    // QP handle, for referencing once.
            
            const uint32_t arg_nid,                             // Node ID
            const uint32_t arg_npl,

            const uint32_t arg_current_sp                       // Sub partition, (in case config changes.)
        ) {
            
            std::string log_fname = "soren_replicator_wt_" + std::to_string(arg_hdl) + ".log";
            std::string logger_name = "SOREN/RELPICATOR/W" + std::to_string(arg_hdl);
            LoggerFileOnly worker_logger(logger_name, log_fname);

            //
            // Local Memory Region (Buffer) tracker.
            uint32_t    n_prop = 0;

            // Memory Region stores its actual log starting the offset 128 bytes.
            // Areas from 0 to 127 bytes are reserved for general purpose RDMA communication
            // buffer. This is necessary since nodes never use TCP anymore (after Hartebeest 
            // exchange).
            int32_t     mr_offset = 128;
            int32_t     mr_linfree = BUF_SIZE - 128;

            //
            // Prepare resources for RDMA operations.
            uint32_t wrkr_mr_id     = GET_MR_GLOBAL(arg_nid, arg_hdl);
            struct ibv_mr* wrkr_mr  = arg_mr_hdls.find(wrkr_mr_id)->second;
            struct ibv_qp** qps     = new struct ibv_qp*[arg_npl]; 
            struct ibv_mr** mrs     = new struct ibv_mr*[arg_npl]; // Holds remote mrs.

            SOREN_LOGGER_INFO(worker_logger, "Replicator({}) initiated.", arg_hdl);

            //
            // For each node ID, register the remote MR information and corresponding local
            //  Queue Pairs.
            for (int nid = 0; nid < arg_npl; nid++) {
                if (nid == arg_nid) {

                    // For this node, it never does comminication (self?).
                    qps[nid] = nullptr; 
                    mrs[nid] = nullptr; 

                } else {
                    qps[nid] = arg_qp_hdls.find(GET_QP_REPLICATOR(arg_nid, nid, arg_current_sp))->second;
                    mrs[nid] = arg_mr_hdls.find(GET_MR_GLOBAL(nid, arg_current_sp))->second;
                }
            }

            //
            // Since it is initiated, peek at others' metadata area to see if something is on them.
            //  This observes other Replayers threads' offset and proposal number.
            //  This may be necessary since a replicator may have respawned.
            struct LogStat* log_stat = reinterpret_cast<struct LogStat*>(wrkr_mr->addr);
            std::memset(log_stat, 0, sizeof(struct LogStat));

            // 
            // Send POST
            
            // This is a Test
            // for (int nid = 0; nid < arg_npl; nid++)
            //     if (nid != arg_nid)
            //         __testRdmaWrite(
            //             qps[nid], wrkr_mr, mrs[nid], arg_nid, 0
            //         );
            // sleep(3600);

            for (int nid = 0; nid < arg_npl; nid++) {
                if (nid == arg_nid) continue;

                while (1) {
                    if (rdmaPost(IBV_WR_RDMA_READ,                      // Do RDMA read
                            qps[nid],                                   // Local Replicator's Queue Pair
                            reinterpret_cast<uintptr_t>(wrkr_mr->addr), // Local buffer address
                            sizeof(struct LogStat),                     // Buffer size
                            wrkr_mr->lkey,                              // Local MR LKey
                            reinterpret_cast<uintptr_t>(mrs[nid]->addr),// Remote's address
                            mrs[nid]->rkey                              // Remotes RKey
                            )
                        != 0) {
                        
                        SOREN_LOGGER_ERROR(worker_logger, "Replicator({}) RDMA Read for LogStat failed.", arg_hdl);
                    }
                    else {
                        waitSingleSCqe(qps[nid]);   // RDMA Read may be unsuccessful.
                                                    // Wait for the Send Completion Queue to be notified by this caller.

                        if (n_prop < log_stat->n_prop) {
                            n_prop = log_stat->n_prop;
                            mr_offset = log_stat->offset;
                        }

                        //
                        // This is a message from the replayer threads. If the message do not match,
                        //  this thread may have read wrong values. Thus, try again the RDMA read to
                        //  get the offset from the replayers.
                        if (log_stat->dummy1 == REPLAYER_READY) {
                            SOREN_LOGGER_INFO(worker_logger, "Reading from ({}):\n- at remote [{}]\n- offset: {}, prop: {}\n- msg: {}", 
                                nid, mrs[nid]->addr, (uint32_t)log_stat->offset, (uint32_t)log_stat->n_prop, (uint32_t)log_stat->dummy1);

                            break;
                        }
                    }
                }
            }

            SOREN_LOGGER_INFO(worker_logger, "Replicator({}) initialized to:\n- offset({}), prop({}).", arg_hdl, mr_offset, n_prop);

            arg_finn_proc_sidx.store(128);  // Store non-zero.

            arg_sig.store(SIG_READY);

            // 
            // Worker handle also represents corresponding sub-partition. 
            // Thus, be sure not to kill any workers arbirarily.
            bool disp_msg = false;
            int32_t signal = 0, rnd_canary = 0;
            u_char next_free_sidx = 0, finn_proc_sidx = 0;
            
            std::srand(std::time(nullptr));

            while (1) {

                signal = arg_sig.load();
                rnd_canary = std::rand();

                switch (signal) {
                    case SIG_PAUSE: 
                        if (disp_msg == false) {
                            SOREN_LOGGER_INFO(worker_logger, "Replicator({}) pending...", arg_hdl);
                            disp_msg = true;
                        }
                        break;

                    case SIG_SELFRET:

                        delete[] qps;
                        delete[] mrs;

                        SOREN_LOGGER_INFO(worker_logger, "Replicator({}) terminated", arg_hdl);
                        return 0;

                    // case SIG_PROPOSE:
                    default:

                        disp_msg = false;
                        next_free_sidx = arg_next_free_sidx.load();
                        next_free_sidx--;

                        while (next_free_sidx != finn_proc_sidx) {
                            n_prop += 1;
                            
                            {
                                finn_proc_sidx++;

                                uintptr_t local_mr_addr = reinterpret_cast<uintptr_t>(wrkr_mr->addr);
                                uintptr_t msg_base = local_mr_addr + mr_offset;
                                uintptr_t remote_mr_addr;

                                // Slot header = arg_slots[idx];
                                Slot* header = &(arg_slots[finn_proc_sidx]);
                                header->n_prop = n_prop;
                                header->canary = rnd_canary;

                                SlotCanary slot_canary = { .canary = rnd_canary };

                                //
                                // Make payload!
                                // 1. Alignment
                                prepareNextAlignedOffset(mr_offset, mr_linfree, header->size);

                                SOREN_LOGGER_INFO(worker_logger, 
                                    "Replicator({}) PROPOSING...\n- current processing slot idx: {}, prop: {}\n- offset: {}, canary: {}\n- content: {}", 
                                        arg_hdl, finn_proc_sidx, (uint32_t)header->n_prop, mr_offset, (uint32_t)header->canary, (char*)header->addr);

                                // 2. Prepare header
                                std::memcpy(
                                    reinterpret_cast<void*>(msg_base),
                                    reinterpret_cast<void*>(header),
                                    sizeof(struct Slot)
                                );

                                // SOREN_LOGGER_INFO(worker_logger, 
                                //     "MEMCPY CHECK:\n- prop: {}, size: {}, canary: {}",
                                //     (uint32_t)reinterpret_cast<Slot*>(msg_base)->n_prop,
                                //     (uint32_t)reinterpret_cast<Slot*>(msg_base)->size,
                                //     (uint32_t)reinterpret_cast<Slot*>(msg_base)->canary
                                //     );

                                // 3. Fetch to the local memory region.
                                std::memcpy(
                                    reinterpret_cast<void*>(msg_base + sizeof(struct Slot)), // Destination   
                                    reinterpret_cast<void*>(header->addr),   // Source
                                    header->size      // Size, dah.
                                );

                                // 4. Prepare slot canary.
                                std::memcpy(
                                    reinterpret_cast<void*>(msg_base + sizeof(struct Slot) + header->size),
                                    &slot_canary,
                                    sizeof(struct SlotCanary)
                                );

                                // 
                                // Send POST
                                for (int nid = 0; nid < arg_npl; nid++) {
                                    if (nid == arg_nid) continue;
                                    
                                    // Update to per-remote contents.
                                    remote_mr_addr = reinterpret_cast<uintptr_t>(mrs[nid]->addr);

                                    reinterpret_cast<struct Slot*>(msg_base)->addr
                                        = remote_mr_addr + mr_offset + sizeof(struct Slot);

                                    std::atomic_thread_fence(std::memory_order_release);

                                    if (rdmaPost(
                                            IBV_WR_RDMA_WRITE, 
                                            qps[nid],               // Local Replicator's Queue Pair
                                            msg_base,               // Local buffer address
                                            (sizeof(struct Slot) 
                                                + header->size + sizeof(struct SlotCanary)),              
                                                                    // Buffer size
                                            wrkr_mr->lkey,          // Local MR LKey
                                            remote_mr_addr + mr_offset,
                                                                    // Remote's address
                                            mrs[nid]->rkey          // Remotes RKey
                                            )
                                        != 0)
                                        SOREN_LOGGER_ERROR(worker_logger, "Replicator({}) RDMA Write failed.", arg_hdl);

                                    if (waitSingleSCqe(qps[nid]) == -1)
                                        SOREN_LOGGER_ERROR(worker_logger, "Replicator({}) RDMA Write failed. (CQ)", arg_hdl);;
                                }

                                mr_offset += (sizeof(struct Slot) + header->size + sizeof(struct SlotCanary));
                                mr_linfree = BUF_SIZE - mr_offset;

                                log_stat->offset = mr_offset;
                                log_stat->n_prop = n_prop;

                                std::memset(header, 0, sizeof(struct Slot));
                                std::memset(
                                    reinterpret_cast<void*>(msg_base), 
                                    0, (sizeof(struct Slot) + header->size + sizeof(struct SlotCanary)));
                            }

                            arg_finn_proc_sidx.store(finn_proc_sidx);
                        }

                        arg_sig.store(SIG_READY);
                }
            }          
                
            return 0;
        }, 
            std::ref(wrkr_inst.wrk_sig), 
            std::ref(wrkr_inst.next_free_sidx), 
            std::ref(wrkr_inst.finn_proc_sidx), 
            wrkr_inst.wrkspace, 
            handle,
            std::ref(mr_hdls), 
            std::ref(qp_hdls),
            node_id, arg_nplayers, arg_cur_sp
    );

    wrkr_inst.wrkt        = std::move(player_thread);
    wrkr_inst.wrkt_nhdl   = wrkr_inst.wrkt.native_handle();

    wrkr_inst.wrkt.detach();
    __waitWorkerSignal(handle, SIG_READY);
    
    return handle;
}


void soren::Replicator::doPropose(uint8_t* arg_addr, size_t arg_size, uint16_t arg_keypref) {

    // Here eveything will be replicated.
    // Sub partition can 8 max.
    uint32_t owner_hdl = 0;
    if (sub_par > 1)
        owner_hdl = arg_keypref % sub_par;
    else
        ;

    while (workers.at(owner_hdl).outstanding.load() == MAX_NSLOTS)
        ;   // Wait until an empty seat is there.

    //
    // Working with slots
    Slot* workspace = workers.at(owner_hdl).wrkspace;
    u_char slot_idx = workers.at(owner_hdl).next_free_sidx.fetch_add(1);   // app handle.

    // Set the local buffer, and let the worker handle the slot.
    workspace[slot_idx].addr = reinterpret_cast<uintptr_t>(arg_addr);
    workspace[slot_idx].size = arg_size;

    workers.at(owner_hdl).outstanding.fetch_add(1);

    while (workers.at(owner_hdl).finn_proc_sidx.load() == slot_idx)
        ;

    workers.at(owner_hdl).outstanding.fetch_sub(1);
}