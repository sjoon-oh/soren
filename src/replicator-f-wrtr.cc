/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */


#include "timer.hh"

#include "logger.hh"
#include "replicator.hh"

#include "commons.hh"
#include "connector.hh"
#include "hartebeest-wrapper.hh"

#include <vector>
#include <infiniband/verbs.h>

#include <cstdio>
#include <iostream>


const int32_t BATCH_SZ = 4;

int workerfDivWriter(
    soren::WorkerThread& arg_wrkr_inst,
    soren::LocalSlot* arg_slots,
    const int arg_hdl,

    soren::DependencyChecker& arg_depchecker,
    
    // Local handles.
    std::map<uint32_t, struct ibv_mr*>& arg_mr_hdls,    // MR handle, for referencing once.
    std::map<uint32_t, struct ibv_qp*>& arg_qp_hdls,    // QP handle, for referencing once.
    
    const uint32_t arg_nid,                             // Node ID
    const uint32_t arg_npl
) {

    std::string log_fname = "soren_writer_wt_" + std::to_string(arg_hdl) + ".log";
    std::string logger_name = "SOREN/WRITER/W" + std::to_string(arg_hdl);

    // soren::LoggerFileOnly worker_logger(logger_name, log_fname);
    soren::Logger worker_logger(logger_name, log_fname);

    uint32_t    n_prop = 0;
    size_t      mr_offset = 128;
    size_t      mr_linfree = soren::BUF_SIZE - 128;
    // Memory Region stores its actual log starting the offset 128 bytes.
    // Areas from 0 to 127 bytes are reserved for general purpose RDMA communication
    // buffer. This is necessary since nodes never use TCP anymore (after Hartebeest 
    // exchange).
    
    // Prepare resources for RDMA operations.
    uint32_t wrkr_mr_id     = GET_MR_GLOBAL(arg_nid, arg_hdl);
    struct ibv_mr* wrkr_mr  = arg_mr_hdls.find(wrkr_mr_id)->second;
    struct ibv_qp** qps     = new struct ibv_qp*[arg_npl]; 
    struct ibv_mr** mrs     = new struct ibv_mr*[arg_npl]; // Holds remote mrs.

    // For each node ID, register the remote MR information and corresponding local
    //  Queue Pairs.
    for (int nid = 0; nid < arg_npl; nid++) {
        if (nid == arg_nid) {
            qps[nid] = nullptr; // For this node, it never does comminication (self?).
            mrs[nid] = nullptr; 

        } else {
            qps[nid] = arg_qp_hdls.find(GET_QP_REPLICATOR(arg_nid, nid, soren::DIV_WRITER))->second;
            mrs[nid] = arg_mr_hdls.find(
                REMOTE_REPLAYER_MR_2_LOCAL(GET_MR_GLOBAL(nid, soren::DIV_WRITER), nid, soren::DIV_WRITER))->second;
        }
    }

    // Since it is initiated, peek at others' metadata area to see if something is on them.
    //  This observes other Replayers threads' offset and proposal number.
    //  This may be necessary since replayers may have respawned.
    struct soren::LogStat* log_stat = reinterpret_cast<struct soren::LogStat*>(wrkr_mr->addr);
    std::memset(log_stat, 0, sizeof(struct soren::LogStat));

    for (int nid = 0; nid < arg_npl; nid++) {
        if (nid == arg_nid) continue;

        while (1) {
            if (soren::rdmaPost(IBV_WR_RDMA_READ, qps[nid], reinterpret_cast<uintptr_t>(wrkr_mr->addr), sizeof(struct soren::LogStat),
                        wrkr_mr->lkey, reinterpret_cast<uintptr_t>(mrs[nid]->addr),mrs[nid]->rkey
                    )
                != 0) {
                    SOREN_LOGGER_ERROR(worker_logger, "Replicator({}) RDMA Read for LogStat failed.", arg_hdl);
            }
            else {
                soren::waitSingleSCqe(qps[nid]);   
                                            // RDMA Read may be unsuccessful.
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
                if (log_stat->dummy1 == soren::REPLAYER_READY) {
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
    arg_wrkr_inst.wrk_sig.store(soren::SIG_READY);   

    int32_t signal = 0;
    uint32_t curproc_idx = 0;   // The slot index are tracked locally, starts from 1 as default.
                                // It is better to let it not exposed to the application thread 
                                // (that calls doPropose() which waits for its request to finish).
                                // The finished slot index (finn_proc_sidx) is only exposed to the
                                // spawner.

    // Batch related
    int32_t cur_batchsz = 0;
    int32_t early_batch = BATCH_SZ;

    size_t batch_mr_ofs[BATCH_SZ] = { 0, };
    uintptr_t batch_msg_base[BATCH_SZ] = { 0, };
    size_t batch_msgsz = 0;

    soren::LocalSlot* local_slot;
    uint32_t batch_slotidx[BATCH_SZ] = { 0, };

    TIMESTAMP_T latest_batch_ts;
    TIMESTAMP_T curr_ts;
    uint64_t elapsed_ts;

    GET_TIMESTAMP(latest_batch_ts);

    int cq_retcode = 0;

    //
    // -- The main loop starts from here. --
    while (1) {

        signal = arg_wrkr_inst.wrk_sig.load();      // Load signal.

        switch (signal) {
            case soren::SIG_PAUSE: 
                break;

            case soren::SIG_SELFRET:

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

                GET_TIMESTAMP(curr_ts);
                elapsed_ts = ELAPSED_NSEC(latest_batch_ts, curr_ts);

                if (elapsed_ts > (10000 * BATCH_SZ)) {
                    
                    if (early_batch > 1) early_batch = cur_batchsz;
                    if (early_batch == 0)
                        early_batch = 1;
                }

                // SOREN_LOGGER_INFO(worker_logger, "early_batch ({}) cur_batchsz ({}).", early_batch, cur_batchsz);

                while (arg_slots[curproc_idx].ready) {
                    n_prop += 1;
                    
                    // local_mr_addr indicates the starting address of MR,
                    // and msg_base indicates the starting address that the replicated data should
                    // be stored.
                    //
                    // Set the info.

                    uintptr_t local_mr_addr     = reinterpret_cast<uintptr_t>(wrkr_mr->addr);
                    uintptr_t msg_base;
                    // uintptr_t remote_mr_addr;

                    local_slot                  = &(arg_slots[curproc_idx]);
                    soren::SlotCanary slot_canary;
                    uint32_t hashed_key;

                    //
                    // 0. Before doing anything, check first whether it is deleted or not.                                
                    // The first case: This slot is already replicated.
                    if (local_slot->footprint == soren::FOOTPRINT_REPLICATED) {
                        SOREN_LOGGER_INFO(worker_logger, "DepCheck: This slot({}) has been replicated.", curproc_idx);

                        arg_slots[curproc_idx].ready = false;
                        arg_slots[soren::MAX_NSLOTS - 1].procs += 1;

                        curproc_idx = (curproc_idx == (soren::MAX_NSLOTS - 1)) ? 0 : curproc_idx + 1;
                        continue;
                    }
                        
                    if (IS_MARKED_AS_DELETED(local_slot->next_slot)) {

                        SOREN_LOGGER_ERROR(worker_logger, "DepCheck: Deleted slot detected for slot idx({}), finding latest one...", curproc_idx);
                        
                        local_slot = arg_depchecker.getNextValidSlot(local_slot);
                        if (local_slot == nullptr) {
                            SOREN_LOGGER_ERROR(worker_logger, "DepCheck: Cannot find valid slot. Continueing...");
                        }

                        arg_slots[curproc_idx].ready = false;
                        arg_slots[soren::MAX_NSLOTS - 1].procs += 1;

                        curproc_idx = (curproc_idx == (soren::MAX_NSLOTS - 1)) ? 0 : curproc_idx + 1;

                        local_slot->footprint = soren::FOOTPRINT_REPLICATED;   // Mark as invalid. 
                        continue;
                    }

                    // SOREN_LOGGER_INFO(worker_logger, "DepCheck ended for current processing index: ({})", curproc_idx);

                    local_slot->header.n_prop   = n_prop;
                    local_slot->header.canary   = local_slot->hashed_key;
                    slot_canary.canary          = local_slot->hashed_key
                                                    + local_slot->header.mem_size
                                                    + local_slot->header.key_size
                                                    + local_slot->header.n_prop;
                    hashed_key                  = local_slot->hashed_key;

                    //
                    // 1. Set message base.
                    // soren::prepareNextAlignedOffset(mr_offset, mr_linfree, local_slot->header.mem_size);
                    msg_base = local_mr_addr + mr_offset; 

                    //
                    // 2. Prepare header. 
                    //  The header is nothing but a copy of a Slot.
                    std::memcpy(
                        reinterpret_cast<void*>(msg_base),
                        reinterpret_cast<void*>(&(local_slot->header)),
                        sizeof(struct soren::HeaderSlot)
                    );

                    //
                    // 3. Fetch to the local memory region.
                    //  Slot currently has a local memory address that the should-be-replicated data sits.
                    //  Third stop fetches the data of the address into the local Memory Region.
                    std::memcpy(
                        reinterpret_cast<void*>(msg_base + sizeof(struct soren::HeaderSlot)),  // Destination   
                        reinterpret_cast<void*>(local_slot->header.mem_addr),               // Source
                        local_slot->header.mem_size                                         // Size, dah.
                    );

                    //
                    // 4. Prepare slot canary.
                    //  Set the last section of a continuous region to Canary.
                    std::memcpy(
                        reinterpret_cast<void*>(msg_base + sizeof(struct soren::HeaderSlot) + local_slot->header.mem_size),
                        &slot_canary,
                        sizeof(struct soren::SlotCanary)
                    );

                    batch_msg_base[cur_batchsz] = msg_base;
                    batch_mr_ofs[cur_batchsz] = mr_offset;
                    
                    batch_slotidx[cur_batchsz] = curproc_idx;

                    cur_batchsz += 1;

                    mr_offset += (sizeof(struct soren::HeaderSlot) + local_slot->header.mem_size + sizeof(struct soren::SlotCanary));
                    mr_linfree = (soren::BUF_SIZE - mr_offset);

                    //
                    // 5. Prepare the next offset
                    soren::prepareNextAlignedOffset(mr_offset, mr_linfree, local_slot->header.mem_size);

                    // SOREN_LOGGER_INFO(worker_logger, "Next offset mr_off: {}", mr_offset);

                    log_stat->offset = mr_offset;
                    log_stat->n_prop = n_prop;

                    // SOREN_LOGGER_INFO(worker_logger, "Item recorded: slot idx {}, current batch: {}", curproc_idx, cur_batchsz);

                    curproc_idx = (curproc_idx == (soren::MAX_NSLOTS - 1)) ? 0 : curproc_idx + 1;

                    if ((curproc_idx == 0)                  // Case 1: Needs to be cleaned up
                        || (cur_batchsz == BATCH_SZ)        // Case 2: cur_batchsz hits the limit
                        || (cur_batchsz > early_batch)      // Case 3: cur_batchsz exceeds the early_batch
                        || (mr_offset == 128)               // Case 4: When circled.
                        ) {
                        early_batch = cur_batchsz;
                        break;
                    }
                }

                if (cur_batchsz == early_batch) {

                    uintptr_t remote_mr_addr;
                    // SOREN_LOGGER_INFO(worker_logger, "Batch sent at: {}", cur_batchsz);

                    for (int nid = 0; nid < arg_npl; nid++) {
                        if (nid == arg_nid) continue;
                        
                        //
                        // Update to per-remote contents. Currently the header section (of MR) holds
                        // local buffer address. This should be updated to the address of remote MR, 
                        // that points to the starting address of the replicated data.
                        remote_mr_addr = reinterpret_cast<uintptr_t>(mrs[nid]->addr);

                        for (int batch_idx = 0; batch_idx < early_batch; batch_idx++) {

                            // SOREN_LOGGER_INFO(worker_logger, "Recovering... idx {} msg_base: {}, mr_off: {}", batch_idx, batch_msg_base[batch_idx], batch_mr_ofs[batch_idx]);

                            uintptr_t local_base = batch_msg_base[batch_idx];
                            reinterpret_cast<struct soren::HeaderSlot*>(local_base)->mem_addr
                                = remote_mr_addr + batch_mr_ofs[batch_idx] + sizeof(struct soren::HeaderSlot);

                            uintptr_t key_addr = reinterpret_cast<struct soren::HeaderSlot*>(local_base)->key_addr;
                            uintptr_t mem_addr = reinterpret_cast<struct soren::HeaderSlot*>(local_base)->mem_addr;

                            if (key_addr == 0)
                                reinterpret_cast<struct soren::HeaderSlot*>(local_base)->key_addr = 0;
                            else
                                reinterpret_cast<struct soren::HeaderSlot*>(local_base)->key_addr
                                    = remote_mr_addr + batch_mr_ofs[batch_idx] + sizeof(struct soren::HeaderSlot) 
                                        + (key_addr - mem_addr);
                        }

                        batch_msgsz = ((batch_msg_base[early_batch - 1] - batch_msg_base[0]) 
                                        + (sizeof(struct soren::HeaderSlot) 
                                            + local_slot->header.mem_size 
                                            + sizeof(struct soren::SlotCanary)));

                        if (soren::rdmaPost(
                                IBV_WR_RDMA_WRITE, 
                                qps[nid],                   // Local Replicator's Queue Pair
                                // msg_base,                   // Local buffer address
                                batch_msg_base[0],
                                // (sizeof(struct soren::HeaderSlot) 
                                //     + local_slot->header.mem_size + sizeof(struct soren::SlotCanary)),   
                                batch_msgsz,
                                                            // Buffer size
                                wrkr_mr->lkey,              // Local MR LKey
                                // remote_mr_addr + mr_offset,
                                (remote_mr_addr + batch_mr_ofs[0]),
                                                            // Remote's address
                                mrs[nid]->rkey              // Remotes RKey
                                )
                            != 0)
                            SOREN_LOGGER_ERROR(worker_logger, "Replicator({}) RDMA Write failed.", arg_hdl);

                        // Wait until the RDMA write is finished.
                        if ((cq_retcode = soren::waitSingleSCqeStatus(qps[nid])) != 0) {
                            SOREN_LOGGER_ERROR(worker_logger, "RDMA Write failed. (CQ Status: {})", arg_hdl, cq_retcode);
                            SOREN_LOGGER_ERROR(worker_logger, "Current stat: \nbatch_msgsz({}), cur_batchsz({})", batch_msgsz, cur_batchsz);
                            SOREN_LOGGER_ERROR(worker_logger, "Current stat: \nmr_offset({}), mr_linfree({})", mr_offset, mr_linfree);

                            for (int batch_idx = 0; batch_idx < early_batch; batch_idx++) {

                                SOREN_LOGGER_ERROR(worker_logger, "  base({}), ofs({})", batch_msg_base[batch_idx], batch_mr_ofs[batch_idx]);
                            }
                            abort();
                        }
                    }

                    //
                    // Release
                    for (int batch_idx = 0; batch_idx < early_batch; batch_idx++) {

                        arg_slots[batch_slotidx[batch_idx]].ready = false;
                        arg_slots[batch_slotidx[batch_idx]].footprint = soren::FOOTPRINT_REPLICATED;

                        arg_slots[soren::MAX_NSLOTS - 1].procs += 1;
                    }

                    cur_batchsz = 0;
                    early_batch = BATCH_SZ;

                    GET_TIMESTAMP(latest_batch_ts);
                }

                arg_wrkr_inst.wrk_sig.store(soren::SIG_READY);
        }
    }

    return 0;

}