/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "logger.hh"
#include "replayer.hh"

#include "commons.hh"
#include "connector.hh"
#include "hartebeest-wrapper.hh"

#include <infiniband/verbs.h>

#include <iostream>

namespace soren {
    static Logger REPLAYER_LOGGER("SOREN/REPLAYER", "soren_replayer.log");
}

void soren::Replayer::__sendWorkerSignal(uint32_t arg_hdl, int32_t arg_sig) {

    workers.at(arg_hdl).wrk_sig.store(arg_sig);

    if (arg_sig == SIG_SELFRET) {
        // Clean up
        workers.at(arg_hdl).wrkt_nhdl = 0;
    }
}

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

soren::Replayer::Replayer(uint32_t arg_nid, uint16_t arg_players, uint32_t arg_ranger = 10, uint32_t arg_subpar = 1) : 
    node_id(arg_nid), nplayers(arg_players), ranger(arg_ranger), sub_par(arg_subpar) {
    if (arg_subpar > MAX_SUBPAR)
        sub_par = MAX_SUBPAR;
}

soren::Replayer::~Replayer() { }

bool soren::Replayer::doAddLocalMr(uint32_t arg_id, struct ibv_mr* arg_mr) {

    if (mr_hdls.find(arg_id) != mr_hdls.end()) return false;
    mr_hdls.insert(
        std::pair<uint32_t, struct ibv_mr*>(arg_id, arg_mr));
    
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

int soren::Replayer::doLaunchPlayer(uint32_t arg_from_nid, int arg_cur_sp) {

    int handle = __findEmptyWorkerHandle();
    WorkerThread& wrkr_inst = workers.at(handle);

    //
    // Initiate a worker thread here.
    std::thread player_thread(
        [](
            std::atomic<int32_t>& arg_sig,                      // Signal
            
            Slot* arg_slots,
            const int arg_hdl,                                  // Worker Handle
            
            // Local handles.
            std::map<uint32_t, struct ibv_mr*>& arg_mr_hdls,  // MR handle, for reference once.
            
            const uint32_t arg_nid,                             // Node ID
            const uint32_t arg_from_nid,
            const int arg_current_sp
        ) {
            
            std::string log_fname = "soren_replayer_wt_" + std::to_string(arg_hdl) + ".log";
            std::string logger_name = "SOREN/REPLAYER/W" + std::to_string(arg_hdl);
            LoggerFileOnly worker_logger(logger_name, log_fname);

            //
            // Local Memory Region (Buffer) tracker.
            uint32_t    n_prop = 0;

            int32_t     mr_offset = 128;
            int32_t     mr_linfree = BUF_SIZE - 128;

            //
            // Prepare resources for RDMA operations.
            uint32_t wrkr_mr_id = GET_MR_GLOBAL(arg_from_nid, arg_current_sp);
            struct ibv_mr* wrkr_mr = arg_mr_hdls.find(wrkr_mr_id)->second;

            //
            // Since it is initiated, peek at others' metadata area to see if something is on them.
            struct LogStat* log_stat = reinterpret_cast<struct LogStat*>(wrkr_mr->addr);
            std::memset(log_stat, 0, sizeof(struct LogStat));

            // 
            // Let replicator read the data.
            log_stat->offset = mr_offset;
            log_stat->n_prop = n_prop;

            SOREN_LOGGER_INFO(worker_logger, 
                "Replayer({}) initiated:\n- MR({})\n- offset({}), prop({})", 
                arg_hdl, wrkr_mr_id, (uint32_t)log_stat->offset, (uint32_t)log_stat->n_prop);

            // 
            // Worker handle also represents corresponding sub-partition. 
            // Thus, be sure not to kill any workers arbirarily.
            bool disp_msg = false;
            int32_t signal = 0, rnd_canary = 0;

            // Set ready.
            log_stat->dummy1 = REPLAYER_READY;

            arg_sig.store(SIG_READY);

            //
            // Test
            // __testRdmaWrite(nullptr, wrkr_mr, nullptr, arg_nid, 1);

            while (1) {

                signal = arg_sig.load();

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
                            uintptr_t local_mr_addr = reinterpret_cast<uintptr_t>(wrkr_mr->addr);
                            uintptr_t msg_base = local_mr_addr + mr_offset;

                            struct Slot* header
                                = reinterpret_cast<struct Slot*>(msg_base);
                            
                            // 1. Read the header, observe the canary, is the prop valid?
                            int32_t header_canary = header->canary;
                            int32_t slot_canary = 
                                reinterpret_cast<struct SlotCanary*>(
                                    msg_base + sizeof(struct Slot) + header->size)->canary;

                            // SOREN_LOGGER_INFO(worker_logger, 
                            //     "Expected Prop({}): \n- header prop: ({}), size: {}\n- canary header/end: ({})/({})", 
                            //     n_prop + 1, (uint32_t)header->n_prop, (uint32_t)header->size, header_canary, slot_canary);

                            if ((header_canary == slot_canary) && (header->n_prop == (n_prop + 1))) {
                                SOREN_LOGGER_INFO(worker_logger, 
                                    "Prop({}): , Size({})\n- canary detected: ({})\n- content: {}", 
                                    (uint32_t)header->n_prop, (uint32_t)header->size, 
                                    (int32_t)header->canary, (char*)(header->addr));

                                // Do something here, REPLAY!!
                                





                            }
                            else 
                                continue;

                            // 2. Suppose you have passed the replicated content to the application.

                            // 3. Corrupt the received canary, and move on.
                            header->canary << 4;
                            n_prop = header->n_prop;

                            mr_offset += (sizeof(struct Slot) + header->size + sizeof(struct SlotCanary));
                            mr_linfree = BUF_SIZE - mr_offset;

                            prepareNextAlignedOffset(
                                mr_offset, mr_linfree, header->size);
                        }
                        
                        // arg_sig.store(SIG_WORKEND);
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
            node_id, arg_from_nid, arg_cur_sp
    );

    wrkr_inst.wrkt        = std::move(player_thread);
    wrkr_inst.wrkt_nhdl   = wrkr_inst.wrkt.native_handle();

    wrkr_inst.wrkt.detach();

    __waitWorkerSignal(handle, SIG_READY);
    __sendWorkerSignal(handle, SIG_CONT);

    return handle;
}

bool soren::Replayer::isWorkerAlive(uint32_t arg_hdl) {
    return (workers.at(arg_hdl).wrkt_nhdl != 0);
}