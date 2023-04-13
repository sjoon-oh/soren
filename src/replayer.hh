#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
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

        uint32_t                    node_id;
        uint32_t                    nplayers;

        uint32_t                    ranger;
        uint32_t                    sub_par;    // Local sub partitions

        //
        // These resources are just borrowed (copied) version. 
        // Do not let any player to reallocate or release these resources.
        // It is Hartebeest's responsibility to control the RDMA resources.
        std::map<uint32_t, struct ibv_mr*>    mr_hdls{};
        
        std::array<WorkerThread, MAX_NWORKER>   workers;

        void __sendWorkerSignal(uint32_t, int32_t);
        void __waitWorkerSignal(uint32_t, int32_t);

        int __findEmptyWorkerHandle();
        int __findNeighborAliveWorkerHandle(uint32_t);

    public:
        Replayer(uint32_t, uint16_t, uint32_t, uint32_t);
        ~Replayer();

        bool doAddLocalMr(uint32_t, struct ibv_mr*);
        void doResetMrs();

        void doTerminateWorker(uint32_t);   // Soft landing.
        void doForceKillWorker(uint32_t);   // May have unexpected behaviour.

        int doLaunchPlayer(uint32_t, int);
        bool isWorkerAlive(uint32_t);
    };
}