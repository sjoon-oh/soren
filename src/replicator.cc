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
    // static Logger REPLICATOR_LOGGER("SOREN/REPLICATOR", "soren_replicator.log");
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
/// @param arg_subpar 
soren::Replicator::Replicator(uint32_t arg_nid, uint16_t arg_players)
    : node_id(arg_nid), nplayers(arg_players) {
        
        propose_cnt = 0;
        depcheck_cnt = 0;
    }

soren::Replicator::~Replicator() {
    std::cout << "Propose Count: " << propose_cnt << ", Depcheck Count: " << depcheck_cnt << std::endl;
    for (auto& elem: mr_remote_hdls) delete elem; 
}



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
int soren::Replicator::doLaunchPlayer(uint32_t arg_nplayers, int arg_div) {

    int handle = __findEmptyWorkerHandle();         // Find the next index with unused WorkerThread array.
    WorkerThread& wrkr_inst = workers.at(handle);

    // Is valid?
    switch (arg_div) {
        case DIV_WRITER: break;
        case DIV_DEPCHECKER: break;
        default:
            abort();
    }

    if (arg_div == DIV_WRITER) {
        std::thread player_thread(
            workerfDivWriter, 
            std::ref(workers.at(handle)), wrkr_inst.wrkspace, handle,
            std::ref(dep_checker), std::ref(mr_hdls), std::ref(qp_hdls),
            node_id, arg_nplayers
        );

        // Register to the fixed array, workers. 
        wrkr_inst.wrkt        = std::move(player_thread);
        wrkr_inst.wrkt_nhdl   = wrkr_inst.wrkt.native_handle();
    }

    else {
        std::thread player_thread(
            workerfDivDepchecker, 
            std::ref(workers.at(handle)), wrkr_inst.wrkspace, handle,
            std::ref(dep_checker), std::ref(mr_hdls), std::ref(qp_hdls),
            node_id, arg_nplayers
        );

        // Register to the fixed array, workers. 
        wrkr_inst.wrkt        = std::move(player_thread);
        wrkr_inst.wrkt_nhdl   = wrkr_inst.wrkt.native_handle();
    }

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
    uint8_t* arg_memaddr, size_t arg_memsz, uint8_t* arg_keypref, size_t arg_keysz, 
    uint32_t arg_hashval, uint8_t arg_reqtype) {

    uint32_t owner_node = 0, owner_hdl = 0;
    uint32_t hashed_val = 0;

    // propose_cnt++;

    //
    // If the arg_keypref do not hold valid address, say nullptr, the arg_hashval is considered as
    // the valid hash for this request.

    if (arg_keypref == nullptr) {
        hashed_val = arg_hashval;
    }
    else hashed_val = dep_checker.doHash(arg_keypref, arg_keysz);

    owner_node = hashed_val % 3;
    // owner_node = node_id;
    owner_hdl = (owner_node == node_id) ? 0 : 1;

    if (owner_hdl == 1)
        depcheck_cnt++;
    else if (owner_hdl == 0 && arg_reqtype == REQTYPE_REPLICATE)
        propose_cnt++;

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
    workspace[slot_idx].hashed_key              = hashed_val;
    workspace[slot_idx].header.owner            = owner_node;

    //
    // Insert to the dependency-checking hash table.
    // dep_checker.doTryInsert(&workspace[slot_idx], arg_keypref, arg_keysz);
    dep_checker.doTryInsert(&workspace[slot_idx]);

    workspace[slot_idx].ready                   = true;

    if (arg_reqtype == REQTYPE_REPLICATE) {
        SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "doPropose: Waiting for idx: {} owned({}), hash: {}", slot_idx, owner_node, workspace[slot_idx].hashed_key);
        while (workspace[slot_idx].footprint != FOOTPRINT_REPLICATED)
            ;
        SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "doPropose: Wait done for idx: {}", slot_idx);
    }
    else {
        ;
        SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "doPropose: Sending ACK: {} owned({}), hash: {}", slot_idx, owner_node, workspace[slot_idx].hashed_key);
    }

    if (slot_idx == (MAX_NSLOTS - 1)) {
        while (workspace[MAX_NSLOTS - 1].procs != MAX_NSLOTS)
            ;

        dep_checker.doResetAll();
        std::memset(workspace, 0, sizeof(struct LocalSlot) * MAX_NSLOTS);

        workers.at(owner_hdl).next_free_sidx.store(0);
    }

    SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "doPropose: exit for {}", workspace[slot_idx].hashed_key);
}



uint32_t soren::Replicator::doHash(uint8_t* arg_keypref, int arg_sz) {
    return dep_checker.doHash(arg_keypref, arg_sz);
}



void soren::Replicator::doReleaseWait(
    uint8_t* arg_memaddr, size_t arg_memsz, 
    uint8_t* arg_keypref, size_t arg_keysz, uint32_t arg_hashval) {

    // Here eveything will be replicated.
    // Sub partition can 8 max.
    uint32_t owner_node = 0, owner_hdl = 0;

    // owner_node = arg_hashval % 3;
    // owner_node = 1;
    // owner_hdl = (owner_node == node_id) ? 0 : 1;
    owner_hdl = 1;

    LocalSlot* workspace = workers.at(owner_hdl).wrkspace;
    int pending_sidx = -1;

    for (int idx = workspace[MAX_NSLOTS - 1].procs; idx < MAX_NSLOTS; idx++) {
    // for (int idx = (MAX_NSLOTS - 1); idx >= 0; idx--) {

        // SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "Scanning idx: {}", idx);

        if ((workspace[idx].hashed_key == arg_hashval)
            && (workspace[idx].footprint != FOOTPRINT_REPLICATED)) {

            pending_sidx = idx;
            break;
        }
    }
    
    if (pending_sidx == -1) {

        // Case when received ACK, but not proposed by this node. 
        // In this case, do nothing.
        return;
    }

    // Release
    workspace[pending_sidx].header.reqs.req_type = REQTYPE_REPLICATE;

    SOREN_LOGGER_INFO(REPLICATOR_LOGGER, "Wait released for hash: {}", arg_hashval);
}