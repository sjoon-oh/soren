#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdint>
#include <atomic>
#include <thread>

namespace soren {

    //
    // This class periodically increases the heartbeat. 
    // Heartbeat class has two variables: handle_beat and core_beat.
    // The former is open to the outside of the instance.
    // The latter is fixed, and declared as atomic variable for multi-threaded apps
    //  can access.
    // The Heartbeat defines its liveness by comparing the open area handle_beat and its
    //  inner area core_beat values.
    class Heartbeat {
    private:
        uint64_t                handle_beat;                  
        std::atomic<uint64_t>   core_beat;

    public:
        Heartbeat();
        ~Heartbeat();

        // Interfaces
        void doWriteBeat(uint64_t);
        void doSelfPound();
        void doReset();

        uint64_t getHandleBeat() const;
        uint64_t getCoreBeat() const;

        bool isLive() const;
    };

    class HeartbeatLocalRunner {
    private:
        const uint8_t           interval;
        Heartbeat               beater;

        std::thread             runner;
        std::thread::native_handle_type
                                runner_handle;
    
    public:
        HeartbeatLocalRunner(uint8_t);
        ~HeartbeatLocalRunner();

        void doLaunchRunner();
        void doKillRunner();

        uint64_t doPeek() const;
    };


}