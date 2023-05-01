#pragma once
/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */


#include "player.hh"
#include "dependency.hh"

#include <vector>
#include <map>     
#include <array>
#include <infiniband/verbs.h>

namespace soren {
    /* Replicator class
     */
    class Replicator {
    private:

        uint32_t        node_id;
        uint32_t        nplayers;

        uint32_t        ranger;
        uint32_t        sub_par;    // Local sub partitions

        uint32_t        propose_cnt;

        //
        // These resources are just borrowed (copied) version. 
        // Do not let any player to reallocate or release these resources.
        // It is Hartebeest's responsibility to control the RDMA resources.
        //
        std::map<uint32_t, struct ibv_mr*>      mr_hdls{};
        std::map<uint32_t, struct ibv_qp*>      qp_hdls{};

        std::vector<struct ibv_mr*>             mr_remote_hdls{};

        std::array<WorkerThread, MAX_NWORKER>   workers;
        
        DependencyChecker   dep_checker;

        // Every player has fixed number of maximum worker sets.
        // However, every element may not be alive.
        // Think of it as its thread capacity.

        void __sendWorkerSignal(uint32_t, int32_t);     // Set the signal variable.
        void __waitWorkerSignal(uint32_t, int32_t);     
            // Wait for the signal variable to change to the target.

        int __findEmptyWorkerHandle();
        int __findNeighborAliveWorkerHandle(uint32_t);

    public:
        Replicator(uint32_t, uint16_t, uint32_t, uint32_t);
        ~Replicator();

        // Memory Regions (MRs) and Queue Pairs (QPs) are registered to this handle maps:
        //  mr_hdls and qp_hdls. The second element of a pair is a pointer to the allocated
        //  RDMA resource.
        // Added resource can be directly used by the workers.

        bool doAddLocalMr(uint32_t, struct ibv_mr*);    // Register MR resource to the handle map: mr_hdls
        bool doAddLocalQp(uint32_t, struct ibv_qp*);    // Register QP resource to the handle map: qp_hdls
        // Make sure to connect QPs outside of the players.

        bool doAddRemoteMr(uint32_t, struct ibv_mr*);   // This is minimal.

        void doResetMrs();  // Reset the handle map.
        void doResetQps();  // Reset the handle map.

        void doTerminateWorker(uint32_t);   // Soft landing.
        void doForceKillWorker(uint32_t);   // May have unexpected behaviour.

        bool isWorkerAlive(uint32_t);

        // Launch a worker thread.
        int doLaunchPlayer(uint32_t, int);
        void doPropose(uint8_t*, size_t, uint8_t*, size_t);
    };
}