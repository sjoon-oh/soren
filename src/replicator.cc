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

#include <cstdio>
#include <iostream>

//
// Replicator methods
namespace soren {
    static LoggerFileOnly REPLICATOR_LOGGER("SOREN/REPLICATOR", "soren_replicator.log");
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
soren::Replicator::Replicator(uint32_t arg_nid, uint16_t arg_players, uint32_t arg_ranger = 10, uint32_t arg_subpar = 1)
    : node_id(arg_nid), nplayers(arg_players), ranger(arg_ranger), sub_par(arg_subpar) {
        if (arg_subpar > MAX_SUBPAR)
            sub_par = MAX_SUBPAR;
        
        propose_cnt = 0;
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
    
    // SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "MR({})[{}] => MR HDL map", 
    //     arg_id, reinterpret_cast<void*>(arg_mr), arg_mr->addr);

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
    
    // SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "QP({})[{}] => QP HDL map", arg_id, reinterpret_cast<void*>(arg_qp));
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
            WorkerThread& arg_wrkr_inst,

            LocalSlot* arg_slots,                               // Workspace for this worker thread.
            const int arg_hdl,                                  // Worker Handle

            DependencyChecker& arg_depchecker,
            
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
            //  This may be necessary since replayers may have respawned.
            struct LogStat* log_stat = reinterpret_cast<struct LogStat*>(wrkr_mr->addr);
            std::memset(log_stat, 0, sizeof(struct LogStat));

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
                                                    // In this stage, a replicator assumes that there are no failed nodes 
                                                    // among the initially registered nodes (according to the exported RDMA
                                                    // network context, Hartebeest.)

                        if (n_prop < log_stat->n_prop) {
                            n_prop = log_stat->n_prop;      // Set the proposal value
                            mr_offset = log_stat->offset;   // Set the initial offset of a memory region.
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

            // Set the signal ready, and alert the spawner thread.
            // If this is not set, the spawner may wait infinitely.
            arg_wrkr_inst.wrk_sig.store(SIG_READY);    

            // 
            // Worker handle also represents corresponding sub-partition. 
            // Thus, be sure not to kill any workers arbirarily.
            bool disp_msg = false;

            int32_t signal = 0, rnd_canary = 0;
            uint32_t curproc_idx = 0;   // The slot index are tracked locally, starts from 1 as default.
                                        // It is better to let it not exposed to the application thread 
                                        // (that calls doPropose() which waits for its request to finish).
                                        // The finished slot index (finn_proc_sidx) is only exposed to the
                                        // spawner.
            
            std::srand(std::time(nullptr));

            //
            // -- The main loop starts from here. --
            while (1) {

                signal = arg_wrkr_inst.wrk_sig.load();      // Load signal.
                rnd_canary = std::rand();                   // Set the randomized canary value.
                                                            // For a proposal, it is designed to have unique random canary
                                                            // values, but multiple replication messages may have identical ones
                                                            // due to low-latency of RDMA and the number of pending requests.

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

                    //
                    // By default, the worker thread constantly checks the unfinished index.
                    // If found, it continues replicating one by one until it reaches its locally 
                    // tracked next_free_sidx. 
                    // next_free_sidx is loaded from the WorkerThread member next_free_sidx, which
                    // is an atomic variable increased by an application thread.
                    // At the moment the two indices do not match, it stops observing the changed value
                    // next_free_sidx and starts processing until the finn_proc_sidx hits the next_free_sidx.
                    //
                    // To prevent that the atomic value keeps changed constantly (above the slot limit), the
                    // WorkerThread saves the number of outstanding requests that an application can observe
                    // to decide whether it should wait or not.
                    // Refer to the WorkerThread for more information.

                    default:

                        while (arg_slots[curproc_idx].ready) {
                            n_prop += 1;
                            
                            // local_mr_addr indicates the starting address of MR,
                            // and msg_base indicates the starting address that the replicated data should
                            // be stored.
                            //
                            // Set the info.

                            uintptr_t local_mr_addr     = reinterpret_cast<uintptr_t>(wrkr_mr->addr);
                            uintptr_t msg_base          = local_mr_addr + mr_offset;
                            uintptr_t remote_mr_addr;

                            LocalSlot* local_slot       = &(arg_slots[curproc_idx]);
                            SlotCanary slot_canary;

                            //
                            // 0. Before doing anything, check first whether it is deleted or not.                                
                            // The first case: This slot is already replicated.
                            if (local_slot->footprint == FOOTPRINT_REPLICATED) {
                                SOREN_LOGGER_INFO(worker_logger, "DepCheck: This slot({}) has been replicated.", curproc_idx);

                                arg_slots[curproc_idx].ready = false;
                                arg_slots[MAX_NSLOTS - 1].procs += 1;

                                curproc_idx = (curproc_idx == (MAX_NSLOTS - 1)) ? 0 : curproc_idx + 1;
                                continue;
                            }
                                
                            if (IS_MARKED_AS_DELETED(local_slot->next_slot)) {

                                SOREN_LOGGER_ERROR(worker_logger, "DepCheck: Deleted slot detected for slot idx({}), finding latest one...", curproc_idx);
                                
                                local_slot = arg_depchecker.getNextValidSlot(local_slot);
                                if (local_slot == nullptr) {
                                    SOREN_LOGGER_ERROR(worker_logger, "DepCheck: Cannot find valid slot. Continueing...");
                                }

                                arg_slots[curproc_idx].ready = false;
                                arg_slots[MAX_NSLOTS - 1].procs += 1;

                                curproc_idx = (curproc_idx == (MAX_NSLOTS - 1)) ? 0 : curproc_idx + 1;

                                local_slot->footprint = FOOTPRINT_REPLICATED;   // Mark as invalid. 
                                continue;
                            }

                            SOREN_LOGGER_INFO(worker_logger, "DepCheck ended for current processing index: ({})", curproc_idx);

                            local_slot->header.n_prop   = n_prop;
                            local_slot->header.canary   = local_slot->hashed_key;
                            slot_canary.canary          = local_slot->hashed_key;

                            //
                            // 1. Alignment : 64 byte aligned by default.
                            prepareNextAlignedOffset(mr_offset, mr_linfree, local_slot->header.mem_size);

                            SOREN_LOGGER_INFO(worker_logger, 
                                "Replicator({}) PROPOSING...\n- current processing slot idx: {}, prop: {}\n- offset: {}, canary: {}\n- content: {}\n- proc'd: {}\n", 
                                    arg_hdl, curproc_idx, (uint32_t)local_slot->header.n_prop, mr_offset, 
                                    (uint32_t)local_slot->header.canary, (char*)local_slot->header.mem_addr, arg_slots[MAX_NSLOTS - 1].procs);

                            //
                            // 2. Prepare header. 
                            //  The header is nothing but a copy of a Slot.
                            std::memcpy(
                                reinterpret_cast<void*>(msg_base),
                                reinterpret_cast<void*>(&(local_slot->header)),
                                sizeof(struct HeaderSlot)
                            );

                            if (local_slot->header.owner == arg_nid) // Skip if the ownwer is this machine.
                                local_slot->header.reqs.req_type = REQTYPE_REPLICATE; 
                            else 
                                local_slot->header.reqs.req_type = REQTYPE_DEPCHECK_WAIT;

                            //
                            // 3. Fetch to the local memory region.
                            //  Slot currently has a local memory address that the should-be-replicated data sits.
                            //  Third stop fetches the data of the address into the local Memory Region.
                            std::memcpy(
                                reinterpret_cast<void*>(msg_base + sizeof(struct HeaderSlot)),  // Destination   
                                reinterpret_cast<void*>(local_slot->header.mem_addr),               // Source
                                local_slot->header.mem_size                                         // Size, dah.
                            );

                            //
                            // 4. Prepare slot canary.
                            //  Set the last section of a continuous region to Canary.
                            std::memcpy(
                                reinterpret_cast<void*>(msg_base + sizeof(struct HeaderSlot) + local_slot->header.mem_size),
                                &slot_canary,
                                sizeof(struct SlotCanary)
                            );

                            // 
                            // 5. Replicate the data to remotes.
                            for (int nid = 0; nid < arg_npl; nid++) {
                                if (nid == arg_nid) continue;
                                
                                //
                                // Update to per-remote contents. Currently the header section (of MR) holds
                                // local buffer address. This should be updated to the address of remote MR, 
                                // that points to the starting address of the replicated data.
                                remote_mr_addr = reinterpret_cast<uintptr_t>(mrs[nid]->addr);
                                reinterpret_cast<struct HeaderSlot*>(msg_base)->mem_addr
                                    = remote_mr_addr + mr_offset + sizeof(struct HeaderSlot);

                                reinterpret_cast<struct HeaderSlot*>(msg_base)->key_addr
                                    = remote_mr_addr + mr_offset + sizeof(struct HeaderSlot) 
                                        + (local_slot->header.key_addr - local_slot->header.mem_addr);

                                // std::atomic_thread_fence(std::memory_order_release);

                                if (rdmaPost(
                                        IBV_WR_RDMA_WRITE, 
                                        qps[nid],                   // Local Replicator's Queue Pair
                                        msg_base,                   // Local buffer address
                                        (sizeof(struct HeaderSlot) 
                                            + local_slot->header.mem_size + sizeof(struct SlotCanary)),              
                                                                    // Buffer size
                                        wrkr_mr->lkey,              // Local MR LKey
                                        remote_mr_addr + mr_offset,
                                                                    // Remote's address
                                        mrs[nid]->rkey              // Remotes RKey
                                        )
                                    != 0)
                                    SOREN_LOGGER_ERROR(worker_logger, "Replicator({}) RDMA Write failed.", arg_hdl);

                                // Wait until the RDMA write is finished.
                                if (waitSingleSCqe(qps[nid]) == -1)
                                    SOREN_LOGGER_ERROR(worker_logger, "Replicator({}) RDMA Write failed. (CQ)", arg_hdl);
                            }

                            // Wait until the mark gets ACKed.

                            if (local_slot->header.reqs.req_type == REQTYPE_DEPCHECK_WAIT) {
                                SOREN_LOGGER_ERROR(worker_logger, "Waiting depcheck for slot... {}", curproc_idx);
                                while (local_slot->header.reqs.req_type == REQTYPE_REPLICATE)
                                    ;
                                SOREN_LOGGER_ERROR(worker_logger, "Replayer received identical hash for slot: {}", curproc_idx);
                            }

                            mr_offset += (sizeof(struct HeaderSlot) + local_slot->header.mem_size + sizeof(struct SlotCanary));
                            mr_linfree = BUF_SIZE - mr_offset;

                            log_stat->offset = mr_offset;
                            log_stat->n_prop = n_prop;

                            //
                            // Reset the MR.
                            std::memset(
                                reinterpret_cast<void*>(msg_base), 
                                0, (sizeof(struct HeaderSlot) + local_slot->header.mem_size + sizeof(struct SlotCanary)));

                            arg_slots[curproc_idx].ready = false;
                            arg_slots[MAX_NSLOTS - 1].procs += 1;

                            curproc_idx = (curproc_idx == (MAX_NSLOTS - 1)) ? 0 : curproc_idx + 1;

                            local_slot->footprint = FOOTPRINT_REPLICATED;   // Mark as invalid. 
                        }

                        arg_wrkr_inst.wrk_sig.store(SIG_READY);
                }
            }          
                
            return 0;
        }, 
            std::ref(workers.at(handle)), 
            wrkr_inst.wrkspace, 
            handle,
            std::ref(dep_checker),                  // Dependency checker, local member.
            std::ref(mr_hdls), 
            std::ref(qp_hdls),
            node_id, arg_nplayers, arg_cur_sp
    );

    // Register to the fixed array, workers. 
    wrkr_inst.wrkt        = std::move(player_thread);
    wrkr_inst.wrkt_nhdl   = wrkr_inst.wrkt.native_handle();

    // Bye son!
    wrkr_inst.wrkt.detach();
    __waitWorkerSignal(handle, SIG_READY);
    
    return handle;
}



/// @brief Propose a value.
/// @param arg_addr 
/// @param arg_size 
/// @param arg_keypref 
void soren::Replicator::doPropose(
    uint8_t* arg_memaddr, size_t arg_memsz, 
    uint8_t* arg_keypref, size_t arg_keysz, uint8_t arg_reqtype) {

    // Here eveything will be replicated.
    // Sub partition can 8 max.
    uint32_t owner_hdl = 0;
    uint32_t keyval = 0;

    if (arg_keysz < sizeof(uint32_t))
        keyval = static_cast<char>(*arg_keypref);
    else
        keyval = static_cast<uint32_t>(*arg_keypref);

    if (sub_par > 1)
        owner_hdl = keyval % sub_par;
    else
        ;


    uint32_t slot_idx;
    while ((slot_idx = workers.at(owner_hdl).next_free_sidx.fetch_add(1)) >= MAX_NSLOTS)
        ;   // Wait until an empty seat is there.

    //
    // Working with slots:
    // Note that the doPropose function is called from application threads. This implies that the 
    // function can be called by multiple times at once.
    // Evert thread fetch slot_idx (the next free slot index that an application thread can use), 
    // fill the space with the local buffer address and size, and wait for the worker thread to 
    // replicate.
    LocalSlot* workspace = workers.at(owner_hdl).wrkspace;
    // uint32_t slot_idx = workers.at(owner_hdl).prepare_sidx.fetch_add(1);

    // Set the local buffer, and let the worker handle the slot.
    // Since this is the only place that next_free_sidx is increased atomically, no overwrites to
    // the others' slots will happen.

    workspace[slot_idx].header.mem_addr         = reinterpret_cast<uintptr_t>(arg_memaddr);
    workspace[slot_idx].header.mem_size         = arg_memsz;
    workspace[slot_idx].header.key_addr         = reinterpret_cast<uintptr_t>(arg_keypref);
    workspace[slot_idx].header.key_size         = arg_keysz;

    workspace[slot_idx].header.reqs.req_type    = arg_reqtype;

    workspace[slot_idx].header.owner = 1;

    //
    // Insert to the dependency-checking hash table.
    dep_checker.doTryInsert(&workspace[slot_idx], arg_keypref, arg_memsz);

    workspace[slot_idx].ready                   = true;

    if (arg_reqtype == REQTYPE_REPLICATE) {
        SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "PRPOPSE: Waiting for idx: {}", slot_idx);
        while (workspace[slot_idx].footprint != FOOTPRINT_REPLICATED)
            ;
        SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "PRPOPSE: Wait done for idx: {}", slot_idx);
    }

    if (slot_idx == (MAX_NSLOTS - 1)) {
        while (workspace[MAX_NSLOTS - 1].procs != MAX_NSLOTS)
            ;

        dep_checker.doResetAll();
        std::memset(workspace, 0, sizeof(struct LocalSlot) * MAX_NSLOTS);

        workers.at(owner_hdl).next_free_sidx.store(0);
    }
}



void soren::Replicator::doReleaseWait(
    uint8_t* arg_memaddr, size_t arg_memsz, 
    uint8_t* arg_keypref, size_t arg_keysz, uint32_t arg_hashval) {

    // Here eveything will be replicated.
    // Sub partition can 8 max.
    uint32_t owner_hdl = 0;
    uint32_t keyval = 0;

    if (arg_keysz < sizeof(uint32_t))
        keyval = static_cast<char>(*arg_keypref);
    else
        keyval = static_cast<uint32_t>(*arg_keypref);

    if (sub_par > 1)
        owner_hdl = keyval % sub_par;
    else
        ;

    LocalSlot* workspace = workers.at(owner_hdl).wrkspace;
    int pending_sidx = -1;

    for (int idx = (MAX_NSLOTS - 1); idx >= 0; idx--) {

        if ((workspace[idx].hashed_key == arg_hashval)
            && (workspace[idx].footprint != FOOTPRINT_REPLICATED)) {
            pending_sidx = idx;
            break;
        }
    }
    
    // SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "Found: {}", pending_sidx);
    if (pending_sidx = -1) {

        // Case when received ACK, but not proposed by this node. 
        // In this case, do nothing.
        return;
    }

    // Release
    workspace[pending_sidx].header.reqs.req_type = REQTYPE_REPLICATE;

    SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "Wait released for idx: {}", pending_sidx);
}