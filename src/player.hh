#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */
#include <stddef.h>
#include <cstdint>
#include <cstdlib>

#include <mutex>
#include <array>
#include <vector>
#include <map>      

#include <atomic>
#include <thread>

// Resource Representation
#include "partition.hh"
#include "connector.hh"

#include "commons.hh"

#include <infiniband/verbs.h>

// #include <infiniband/verbs.h> // OFED IB verbs

namespace soren {

    const int MAX_NWORKER   = 8;
    const int MAX_SUBPAR    = 8;

    const int MAX_REPLICATOR_WORKER = MAX_NWORKER / 2;
    const int MAX_REPLAYER_WORKER = MAX_NWORKER;

    const int MAX_NSLOTS    = 128;

    enum {
        SIG_PAUSE       = 0x00,
        SIG_SELFRET    = 0x02,
        SIG_CONT,
        SIG_PROPOSE     = 0xff,
        SIG_WORKEND,
        SIG_READY,
    };

    struct log_entry {
        char*       addr;
        uint32_t    size;
    };

    struct WorkerThread {
        std::thread                         wrkt;           // Thread obj
        std::thread::native_handle_type     wrkt_nhdl;      // Native handle.
        std::atomic<int32_t>                wrk_sig;        // Signal Var.

        Slot*                               wrkspace;       // Per-thread workspace.
        std::atomic<int32_t>                ws_free_idx;    // Signal Var.

        WorkerThread() : wrkt_nhdl(0), wrk_sig(0), wrkspace(new Slot[MAX_NSLOTS]), ws_free_idx(1) { }
        WorkerThread(std::thread& arg_t, std::thread::native_handle_type arg_hdl) :
            wrkt(std::move(arg_t)), wrkt_nhdl(arg_hdl), 
            wrk_sig(0), wrkspace(new Slot[MAX_NSLOTS]), ws_free_idx(1) { }

        ~WorkerThread() { delete[] wrkspace; }
    };

    // Simple:
    // Each role have one memory region, one queue pair. 
    class Player {
    protected:
        uint32_t                    node_id;
        
        //
        // These resources are just borrowed (copied) version. 
        // Do not let any player to reallocate or release these resources.
        // It is Hartebeest's responsibility to control the RDMA resources.
        std::map<uint32_t, struct ibv_mr*>    mr_hdls{};
        std::map<uint32_t, struct ibv_qp*>    qp_hdls{};

        std::vector<struct ibv_mr*>           mr_remote_hdls{};
        // This holds minimal infos. The resources are freed at dtor.

        std::array<WorkerThread, MAX_NWORKER>   workers;

        void __sendWorkerSignal(uint32_t, int32_t);
        void __waitWorkerSignal(uint32_t, int32_t);
        // Every player has fixed number of maximum worker sets.
        // However, every element may not be alive.
        // Think of it as its thread capacity.

        // Partition related:
        // Partitioner                 judge;
        std::atomic<uint32_t>       sub_par;    // Local sub partitions

        // Test func.
        
    public: 
        Player(uint32_t, uint16_t, uint32_t, uint32_t);
        ~Player() { for (auto& elem: mr_remote_hdls) delete elem; };

        bool doAddLocalMr(uint32_t, struct ibv_mr*);
        bool doAddLocalQp(uint32_t, struct ibv_qp*);
        // Make sure to connect QPs outside of the players.

        bool doAddRemoteMr(uint32_t, struct ibv_mr*);   // This is minimal.

        void doResetMrs();
        void doResetQps();

        void doTerminateWorker(uint32_t);   // Soft landing.
        void doForceKillWorker(uint32_t);   // May have unexpected behaviour.

        bool isWorkerAlive(uint32_t);
    };
    

    /* Replicator class
     */
    class Replicator final : public Player {
    protected:
        int __findEmptyWorkerHandle();
        int __findNeighborAliveWorkerHandle(uint32_t);

    public:
        Replicator(uint32_t, uint16_t, uint32_t, uint32_t);
        ~Replicator() = default;

        int doLaunchPlayer(uint32_t);
        void doPropose(uint8_t*, size_t, uint16_t = 0);
    };


    /* Replayer class
     */
    class Replayer final : public Player {
    private:        
        int __findEmptyWorkerHandle();
        int __findNeighborAliveWorkerHandle(uint32_t);

    public:
        Replayer(uint32_t, uint16_t, uint32_t, uint32_t);
        ~Replayer();

        int doLaunchPlayer(uint32_t, int);
    };

    int __testRdmaWrite(struct ibv_qp*, struct ibv_mr*, struct ibv_mr*, int, int);
    int __testRdmaRead(struct ibv_qp*, struct ibv_mr*, struct ibv_mr*, int, int);
}