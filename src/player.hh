#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */
#include <stddef.h>
#include <cstdint>

#include <mutex>
#include <vector>
#include <map>      

#include <atomic>
#include <thread>

// Resource Representation
#include "partition.hh"
#include "hartebeest-wrapper.hh"

// #include <infiniband/verbs.h> // OFED IB verbs

namespace soren {

    enum {
        WORKER_CONTINUE,
        WORKER_PAUSE
    };

    struct log_entry {
        char*       addr;
        uint32_t    size;
    };

    struct WorkerThread {
        std::thread                         wrkt;       // Thread obj
        std::thread::native_handle_type     wrkt_nhdl;  // Native handle.

        WorkerThread() : wrkt_nhdl(0) { }
        WorkerThread(std::thread& arg_t, std::thread::native_handle_type arg_hdl) :
            wrkt(std::move(arg_t)), wrkt_nhdl(arg_hdl) { }
    };

    // Simple:
    // Each role have one memory region, one queue pair. 
    class Player {
    protected:
        uint32_t        node_id;
        
        //
        // These resources are just borrowed (copied) version. 
        // Do not let any player to reallocate or release these resources.
        // It is Hartebeest's responsibility to control the RDMA resources.
        std::map<uint32_t, struct ibv_mr*>      mr_hdls{};
        std::map<uint32_t, struct ibv_qp*>      qp_hdls{};

        // std::vector<MemoryRegion>   mr_hdls{};
        // std::vector<QueuePair>      qp_hdls{};  // Queue Pair Handles.

        std::atomic<bool>           stop_and_go;
        std::vector<WorkerThread>   workers;    // This worker vector is append only.
                                                // Killed threads will have its handle set to 0.


        // Partition related:
        Partitioner                 judge;
        std::atomic<uint32_t>       sub_par;    // Local sub partitions

        
    public: 
        Player(uint16_t, uint32_t, uint32_t);
        virtual ~Player() = default;

        // Comm interfaces.
        virtual int doLaunchPlayer(uint32_t, uint32_t) = 0;
        void doContinueAllWorkers();
        void doPauseAllWorkers();
        virtual int doUpdateConfig() = 0;

        bool doAddMr(uint32_t, struct ibv_mr*);
        bool doAddQp(uint32_t, struct ibv_qp*);

        void doResetMrs();
        void doResetQps();

        void doKillWorker(uint32_t);

        bool isWorkerAlive(uint32_t);
    };
    

    /* Replicator class
     */
    class Replicator final : public Player {
    private:

    public:
        Replicator(uint16_t, uint32_t, uint32_t);
        virtual ~Replicator() = default;

        virtual int doLaunchPlayer(uint32_t, uint32_t);
        virtual int doUpdateConfig() { return 0; };;

        int doPropose();
    };


    /* Replayer class
     */
    class Replayer final : public Player {
    private:        

    public:

        virtual int doLaunchPlayer() { return 0; };
        virtual int doUpdateConfig() { return 0; };
    };
}