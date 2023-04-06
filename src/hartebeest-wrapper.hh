#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <mutex>
#include <memory>

#include "rdma-conf.hpp"

namespace soren {
    namespace hbwrapper {

        std::once_flag HB_INIT_FLAG;    // Do not mess up with multiple initializations and clean ups.
        std::once_flag HB_CLEAN_FLAG;

        std::unique_ptr<hartebeest::RdmaConfigurator>       HB_CONFIGURATOR;
        std::unique_ptr<hartebeest::ConfigFileExchanger>    HB_EXCHANGER;

        // Initialize above the two.
        void initHartebeest();
        void cleanHartebeest();

        void initRdmaConfigurator();
        bool initConfigFileExchanger();

        int registerPd(uint32_t);
        uint8_t* allocateBuffer(size_t, int);

        int registerMr(uint32_t, uint32_t, uint8_t*, size_t);
        int registerRcQp(uint32_t, uint32_t, uint32_t, uint32_t);

        int exportLocalRdmaConfig(int);
        bool getReadyForExchange();

        int exchangeRdmaConfigs();
    }
}