#pragma once
/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "player.hh"

#include <map>     
#include <array>
#include <infiniband/verbs.h>

namespace soren {
    /* Replayer class
     */
    class Replayer {
    private:

        uint32_t        node_id;
        uint32_t        nplayers;

        uint32_t        ranger;
        uint32_t        sub_par;    // Local sub partitions

        //
        // These resources are just borrowed (copied) version. 
        // Do not let any player to reallocate or release these resources.
        // It is Hartebeest's responsibility to control the RDMA resources.
        //
        std::map<uint32_t, struct ibv_mr*>    mr_hdls{};
        
        std::array<WorkerThread, MAX_NWORKER>   workers;

        // Every player has fixed number of maximum worker sets.
        // However, every element may not be alive.
        // Think of it as its thread capacity.

        void __sendWorkerSignal(uint32_t, int32_t);     // Set the signal variable.
        void __waitWorkerSignal(uint32_t, int32_t);
            // Wait for the signal variable to change to the target.

        int __findEmptyWorkerHandle();
        int __findNeighborAliveWorkerHandle(uint32_t);

    public:
        Replayer(uint32_t, uint16_t, uint32_t, uint32_t);
        ~Replayer();

        // Memory Regions (MRs) and Queue Pairs (QPs) are registered to this handle maps:
        //  mr_hdls and qp_hdls. The second element of a pair is a pointer to the allocated
        //  RDMA resource.
        // Added resource can be directly used by the workers.
        //
        // Unlike the Replicator, Replayer only requires Memory Region, because it is a passive
        //  player. Replayer never sends or reads other node's data, thus do not require
        //  queue pairs.

        bool doAddLocalMr(uint32_t, struct ibv_mr*);
        void doResetMrs();

        void doTerminateWorker(uint32_t);   // Soft landing.
        void doForceKillWorker(uint32_t);   // May have unexpected behaviour.

        // Launch a worker thread.
        int doLaunchPlayer(uint32_t, int);
        bool isWorkerAlive(uint32_t);
    };
}