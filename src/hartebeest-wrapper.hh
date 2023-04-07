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
        
        int getThisNodeId();
        int getNumPlayers();

        // Get infos/
        struct ::ibv_mr* getMr(uint32_t);
        struct ::ibv_qp* getQp(uint32_t);
    }
}