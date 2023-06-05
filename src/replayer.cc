/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "logger.hh"
#include "replayer.hh"

#include "commons.hh"
#include "connector.hh"
#include "hartebeest-wrapper.hh"

#include "replicator.hh"

#include <infiniband/verbs.h>

#include <cstdio>
#include <iostream>

namespace soren {
    static LoggerFileOnly REPLAYER_LOGGER("SOREN/REPLAYER", "soren_replayer.log");
}



/// @brief Sends a worker signal.
/// @param arg_hdl 
/// @param arg_sig 
void soren::Replayer::__sendWorkerSignal(uint32_t arg_hdl, int32_t arg_sig) {

    workers.at(arg_hdl).wrk_sig.store(arg_sig);

    if (arg_sig == SIG_SELFRET) {
        // Clean up
        workers.at(arg_hdl).wrkt_nhdl = 0;
    }
}



/// @brief Waits until a worker sets the signal.
/// @param arg_hdl 
/// @param arg_sig 
void soren::Replayer::__waitWorkerSignal(uint32_t arg_hdl, int32_t arg_sig) {
    
    auto& signal = workers.at(arg_hdl).wrk_sig;
    while (signal.load() != arg_sig)
        ;
}



int soren::Replayer::__findEmptyWorkerHandle() {
    int hdl;
    for (hdl = 0; hdl < MAX_REPLAYER_WORKER; hdl++) {
        if (workers[hdl].wrkt_nhdl == 0)
            return hdl;
    }

    if (hdl == MAX_NWORKER) return -1;
}



int soren::Replayer::__findNeighborAliveWorkerHandle(uint32_t arg_hdl) {
    
    int hdl = arg_hdl + 1;
    if (arg_hdl == MAX_REPLAYER_WORKER) hdl = 0;

    for ( ; hdl < MAX_REPLAYER_WORKER; hdl++)
        if (isWorkerAlive(hdl))
            return hdl;
    
    return -1;
}



/// @brief Constructor of Replayer instance.
/// @param arg_nid 
/// @param arg_players 
/// @param arg_ranger 
/// @param arg_subpar 
soren::Replayer::Replayer(uint32_t arg_nid, uint16_t arg_players, uint32_t arg_subpar = 1) : 
    node_id(arg_nid), nplayers(arg_players), sub_par(arg_subpar) {
    if (arg_subpar > MAX_SUBPAR)
        sub_par = MAX_SUBPAR;
}

soren::Replayer::~Replayer() { }



/// @brief doAddLocalMr inserts the allocated RDMA MR resource (on this machine).
/// @param arg_id 
/// @param arg_mr 
/// @return 
bool soren::Replayer::doAddLocalMr(uint32_t arg_id, struct ibv_mr* arg_mr) {

    if (mr_hdls.find(arg_id) != mr_hdls.end()) return false;
    mr_hdls.insert(
        std::pair<uint32_t, struct ibv_mr*>(arg_id, arg_mr));

    //
    // Since a worker thread should have its region to work with, 
    //  an MR generated from the Connector instance should be registered here.
    //  All Memory Regions should be registered for each worker thread spawned.
    // Note that the resources are managed by the connector. 
    //  Replicator just borrows it by having its pointer, but should never free.
    //
    // Unlike the Replicator class, Replayer only have doAddLocalMr, since this is 
    // a passive player. Replayer never do RDMA reads or writes. This instnace only 
    // plays with it memory, thus do not have doAddLocalQp.
    
    SOREN_LOGGER_INFO(REPLAYER_LOGGER, "MR({})[{}] => Replayer map", 
        arg_id, reinterpret_cast<void*>(arg_mr), arg_mr->addr);

    return true;
}




void soren::Replayer::doResetMrs() { mr_hdls.clear(); }

void soren::Replayer::doTerminateWorker(uint32_t arg_hdl) { __sendWorkerSignal(arg_hdl, SIG_SELFRET); }
void soren::Replayer::doForceKillWorker(uint32_t arg_hdl) {

    pthread_t native_handle = static_cast<pthread_t>(workers.at(arg_hdl).wrkt_nhdl);
    if (native_handle != 0)
        pthread_cancel(native_handle);

    // Reset all.
    workers.at(arg_hdl).wrkt_nhdl = 0;
    workers.at(arg_hdl).wrk_sig.store(0);
}



/// @brief Launches a worker thread, which handles arg_cur_sp sub partition.
/// @param arg_from_nid 
/// @param arg_cur_sp 
/// @return 
int soren::Replayer::doLaunchPlayer(
        uint32_t arg_from_nid, int arg_cur_sp, Replicator* arg_replicator,
        std::function<int(uint8_t*, size_t, int, void*)> arg_repfunc
    ) {

    int handle = __findEmptyWorkerHandle();         // Find the next index with unused WorkerThread array.
    WorkerThread& wrkr_inst = workers.at(handle);

    //
    // Initiate a worker thread here.
    std::thread player_thread(
        [](
            std::atomic<int32_t>& arg_sig,                      // Signal
            
            LocalSlot* arg_slots,                               // Workspace for this worker thread.
            const int arg_hdl,                                  // Worker Handle
            
            // LocalSlot handles.
            std::map<uint32_t, struct ibv_mr*>& arg_mr_hdls,    // MR handle, for reference once.
            
            const uint32_t arg_nid,                             // Node ID
            const uint32_t arg_from_nid,                        // Node ID that sends data from.
            const int arg_current_sp,                           // Sub partition, (in case config changes.)
            Replicator* arg_replicator,
            std::function<int(uint8_t*, size_t, int, void*)> arg_replayf
        ) {
            
            std::string log_fname = "soren_replayer_wt_" + std::to_string(arg_hdl) + ".log";
            std::string logger_name = "SOREN/REPLAYER/W" + std::to_string(arg_hdl);
            LoggerFileOnly worker_logger(logger_name, log_fname);
            // Logger worker_logger(logger_name, log_fname);

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
            uint32_t wrkr_mr_id = GET_MR_GLOBAL(arg_from_nid, arg_current_sp);
            struct ibv_mr* wrkr_mr = arg_mr_hdls.find(wrkr_mr_id)->second;

            //
            // Since it is initiated, peek at others' metadata area to see if something is on them.
            //  This lets the other Replicator to read threads' offset and proposal number.
            //  This may be necessary since a replicator may have respawned.
            struct LogStat* log_stat = reinterpret_cast<struct LogStat*>(wrkr_mr->addr);
            std::memset(log_stat, 0, sizeof(struct LogStat));

            // 
            // Let replicator read the data.
            log_stat->offset = mr_offset;
            log_stat->n_prop = n_prop;

            SOREN_LOGGER_INFO(worker_logger, 
                "Replayer({}) initiated:\n- MR({})\n- offset({}), prop({})\n- addr: ({})", 
                arg_hdl, wrkr_mr_id, (uint32_t)log_stat->offset, (uint32_t)log_stat->n_prop, (uintptr_t)wrkr_mr->addr);

            // 
            // Worker handle also represents corresponding sub-partition. 
            // Thus, be sure not to kill any workers arbirarily.
            bool disp_msg = false;
            int32_t signal = 0, rnd_canary = 0;

            // Set ready.
            log_stat->dummy1 = REPLAYER_READY;
            
            // This is a message for the replayer threads of other nodes. 
            // If the message do not match, the replicator threads may have read 
            // wrong values. In that case, the threads will try RDMA reads again 
            // to get the offset from the replayers.

            arg_sig.store(SIG_READY);   // Let the caller that I am ready.
                                        // If this is not set, the spawner may wait infinitely.

            //
            // -- The main loop starts from here. --
            while (1) {

                signal = arg_sig.load();    // Load signal.

                switch (signal) {
                    case SIG_PAUSE:
                        if (disp_msg == false) {
                            SOREN_LOGGER_INFO(worker_logger, "Replayer({}) pending...", arg_hdl);
                            disp_msg = true;
                        }
                        break;

                    case SIG_SELFRET:

                        SOREN_LOGGER_INFO(worker_logger, "Replayer({}) terminated", arg_hdl);
                        return 0;

                    case SIG_CONT:
                        disp_msg = false;
                        
                        {   
                            // Since the remote is not alerted that some data are written to its memory,
                            // a replayer thread should poll constantly whether any new data has arrived.
                            // The replicator sets the very first continuous area as Slots, which is fixed thus
                            // the size is predictable.
                            //
                            // Replayer thread keeps observing the header, and according to the size of the data,
                            // it compares the canary value contained in the header Slot with the end SlotCanary
                            // value. If it is valid, then it means that there is something arrived.
                            // To prevent the false alarm, one of the canary values is deliberately corrupted (shifted)
                            // by the replayer.

                            uintptr_t local_mr_addr = reinterpret_cast<uintptr_t>(wrkr_mr->addr);
                            uintptr_t msg_base = local_mr_addr + mr_offset;

                            struct HeaderSlot* header
                                = reinterpret_cast<struct HeaderSlot*>(msg_base);
                            
                            // 1. Read the header, observe the canary, is the prop valid?
                            uint32_t header_canary = header->canary;
                            uint32_t slot_canary = 
                                reinterpret_cast<struct SlotCanary*>(
                                    msg_base + sizeof(struct HeaderSlot) + header->mem_size)->canary;

                            uint32_t received_prop  = header->n_prop;
                            uint32_t calc_canary    = header_canary + header->mem_size + header->key_size + received_prop;

                            // SOREN_LOGGER_INFO(worker_logger, 
                            //     "Expected Prop({}): \n- header prop: ({}), size: {}\n- canary header/end: ({})/({})", 
                            //     n_prop + 1, (uint32_t)header->n_prop, (uint32_t)header->size, header_canary, slot_canary);

                            // SOREN_LOGGER_INFO(worker_logger, "Alive");

                            if ((calc_canary == slot_canary) && (received_prop > n_prop)) {

                                if ((header->mem_addr == 0) || (header->mem_size == 0))
                                    continue;

                                if (header->owner == arg_nid) {
                                    SOREN_LOGGER_INFO(worker_logger, "Mine! Response(propose) for hash: {}, canary: {}, prop: {}", header_canary, calc_canary, n_prop);
                                    
                                    arg_replicator->doPropose(
                                        reinterpret_cast<uint8_t*>(header->mem_addr), header->mem_size,
                                        reinterpret_cast<uint8_t*>(header->key_addr), header->key_size,
                                        header_canary, REQTYPE_DEPCHECK_ACK
                                    );  // Send perspective

                                } else {

                                    // if (header->reqs.req_type == REQTYPE_DEPCHECK_ACK) {

                                    if (header->reqs.req_type != REQTYPE_DEPCHECK_WAIT) {

                                        SOREN_LOGGER_INFO(worker_logger, "Received! {}", header_canary);
                                        arg_replicator->doReleaseWait(
                                            reinterpret_cast<uint8_t*>(header->mem_addr), header->mem_size,
                                            reinterpret_cast<uint8_t*>(header->key_addr), header->key_size, header_canary
                                        );
                                    }
                                    else
                                        SOREN_LOGGER_INFO(worker_logger, "Seems others are waiting for ACK: {}", header_canary);
                                    // }
                                }

                                //
                                // Do something here, REPLAY!!
                                {
                                    if (arg_replayf == nullptr) {

                                        ;
                                    }
                                    else
                                        arg_replayf(
                                            reinterpret_cast<uint8_t*>(header->mem_addr), 
                                            header->mem_size, 
                                            0,
                                            nullptr);
                                }
                            }
                            else 
                                continue;

                            // 2. Suppose you have passed the replicated content to the application.

                            // 3. Corrupt the received canary, and move on.
                            header->canary << 4;
                            n_prop = received_prop;

                            mr_offset += (sizeof(struct HeaderSlot) + header->mem_size + sizeof(struct SlotCanary));
                            mr_linfree = BUF_SIZE - mr_offset;

                            // 4. Set the next aligned offset.
                            prepareNextAlignedOffset(
                                mr_offset, mr_linfree, header->mem_size);

                            log_stat->offset = mr_offset;
                            log_stat->n_prop = n_prop;
                        }
                        
                        break;

                    default:
                        ;
                }
            }
                
            return 0;
        }, 
            std::ref(wrkr_inst.wrk_sig), 
            wrkr_inst.wrkspace, handle,
            std::ref(mr_hdls),
            node_id, arg_from_nid, arg_cur_sp, arg_replicator, arg_repfunc
    );

    // Register to the fixed array, workers. 
    wrkr_inst.wrkt        = std::move(player_thread);
    wrkr_inst.wrkt_nhdl   = wrkr_inst.wrkt.native_handle();

    // Bye son!
    wrkr_inst.wrkt.detach();

    __waitWorkerSignal(handle, SIG_READY);
    __sendWorkerSignal(handle, SIG_CONT);

    return handle;
}



bool soren::Replayer::isWorkerAlive(uint32_t arg_hdl) {
    return (workers.at(arg_hdl).wrkt_nhdl != 0);
}