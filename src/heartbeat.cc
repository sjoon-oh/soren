/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <unistd.h>
#include <thread>

#include "logger.hh"
#include "heartbeat.hh"

namespace soren {
    static Logger HEARTBEAT_LOGGER("SOREN/HB", "soren_hb.log");
}

soren::Heartbeat::Heartbeat() { doReset(); } // CTOR
soren::Heartbeat::~Heartbeat() { } // DTOR

void soren::Heartbeat::doWriteBeat(uint64_t arg_handle_beat) {
    handle_beat = arg_handle_beat;
}

void soren::Heartbeat::doSelfPound() { 
    handle_beat = core_beat.fetch_add(1);
}

void soren::Heartbeat::doReset() {
    handle_beat = 0;
    core_beat.store(0);
}

// Getters
uint64_t soren::Heartbeat::getHandleBeat() const { return handle_beat; }
uint64_t soren::Heartbeat::getCoreBeat() const { return core_beat.load(); }

bool soren::Heartbeat::isLive() const {
    uint64_t cur_beat = core_beat.load();
    return (handle_beat != cur_beat);
}

/* HeartbeatLocalRunner */
soren::HeartbeatLocalRunner::HeartbeatLocalRunner(uint8_t arg_itvl) : interval(arg_itvl) { }
soren::HeartbeatLocalRunner::~HeartbeatLocalRunner() { }

void soren::HeartbeatLocalRunner::doLaunchRunner() {

    std::thread local_runner(
        [](Heartbeat& arg_beater, uint8_t arg_itvl) {
            while (1) {
                usleep(arg_itvl);
                arg_beater.doSelfPound();
            }
        },
        std::ref(beater), interval
    );

    runner = std::move(local_runner);
    runner_handle = runner.native_handle();

    runner.detach();

    SOREN_LOGGER_INFO(HEARTBEAT_LOGGER, "HeartbeatLocalRunner launched: Handle {}", runner_handle);
}

void soren::HeartbeatLocalRunner::doKillRunner() {
    
    SOREN_LOGGER_INFO(HEARTBEAT_LOGGER, "HeartbeatLocalRunner killing: Handle {}", runner_handle);
    pthread_cancel(static_cast<pthread_t>(runner_handle));
}

uint64_t soren::HeartbeatLocalRunner::doPeek() const {
    return beater.getCoreBeat();
}

