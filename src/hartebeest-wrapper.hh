#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <mutex>
#include <memory>

#include <infiniband/verbs.h>

namespace soren {

    namespace hbwrapper {

        // Initialize above the two.
        void initHartebeest();
        void cleanHartebeest();

        void initRdmaConfigurator();
        bool initConfigFileExchanger();

        int registerPd(uint32_t);
        uint8_t* allocateBuffer(size_t, int);

        int registerMr(uint32_t, uint32_t, uint8_t*, size_t);
        int registerRcQp(uint32_t, uint32_t, uint32_t, uint32_t);

        int exchangeRdmaConfigs();

        int connectRcQps(uint32_t, uint32_t, uint32_t, uint32_t);
        
        int getThisNodeId();
        int getNumPlayers();

        // Get infos/
        struct ibv_mr* getLocalMr(uint32_t);
        struct ibv_qp* getLocalQp(uint32_t);

        struct ibv_mr* getRemoteMinimalMr(uint32_t, uint32_t, uint32_t);
        
    }

    int rdmaPost(
        enum ibv_wr_opcode, struct ibv_qp*, 
        uintptr_t, uint32_t, uint32_t, uintptr_t, uint32_t);

    int waitSingleSCqe(struct ibv_qp*);
}
