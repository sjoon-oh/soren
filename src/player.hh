#pragma once
/* github.com/sjoon-oh/soren
 * @author Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 * @brief Worker thread management class.
 */
#include <stddef.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <mutex>
#include <array>
#include <vector>
#include <map>      

#include <atomic>
#include <thread>

// Resource Representation
#include "connector.hh"
#include "commons.hh"

#include <infiniband/verbs.h>

namespace soren {

    const int MAX_NWORKER   = 8;

    const int MAX_REPLICATOR_WORKER = MAX_NWORKER / 2;
    const int MAX_REPLAYER_WORKER = MAX_NWORKER;

    const int MAX_NSLOTS    = 512;

    enum {
        SIG_PAUSE       = 0x00,
        SIG_SELFRET     = 0x02,
        SIG_CONT,
        SIG_PROPOSE     = 0xff,
        SIG_WORKEND,
        SIG_READY,
        SIG_NOTIFIED,
    };

    enum {
        DIV_WRITER      = 0x00,
        DIV_DEPCHECKER  = 0x01
    };

    struct log_entry {
        char*       addr;
        uint32_t    size;
    };

    //
    // WorkerThread manages single thread.
    //  A worker thread (whether it is a Replayer or Replicator) launches multiple
    //  threads. This is managed by a list of WorkerThreads, having a fixed number.
    //  A worker is detached from the spawner (main thread). Since each may contain
    //  an infinite loop, it may be hard to force kill a running thread from the
    //  spawner.
    //
    // For this reason, along with the variable wrkt which saves a std::thread instance,
    //  its native handle wrkt_nhdl is also saved before detaching a thread. Linux defines
    //  the thread::native_handle_type as pthread_t, thus the main thread can forcefully 
    //  kill the thread using this handle (by calling pthread_cancel()).
    struct WorkerThread {

        std::thread                         wrkt;           // Thread obj
        std::thread::native_handle_type     wrkt_nhdl;      // Native handle.
        std::atomic<int32_t>                wrk_sig;        // Signal Var.

        //
        // A spawner thread sets the atomic signal variable.
        // The value is read from the detached thread and do some actions by its value.
        //
        // wrkspace represents a list of Slots, having its fixed maximum number.
        //  The list is set to have 256 elements as default. Each thread has its own
        //  list of Slots to process a PROPOSE request. 
        //
        //  next_free_sidx indicates the next unused slots. This value is read by 
        //  the PROPOSE-request thread, puts its request, and atomically increased by the 
        //  PROPOSE-request thread. A worker thread must not modify the next_free_sidx. 
        //  finn_proc_sidx represents a Slot index that a worker just finished processing. 
        //  This value is read by the PROPOSE-request thread and waits until it reaches the 
        //  value it expects. This value should never be modified by the PROPOSE-request thread.
        //
        // Outstanding indicates the number of requests that wait the worker thread to finish.
        //  This value is read by the PROPOSE-request thread, atomically increased by the PROPOSE-request
        //  thread, and never be modified by a worker thread.
        //

        LocalSlot*                          wrkspace;       // Per-thread workspace.
        std::atomic<uint32_t>               next_free_sidx; // Points to the next index to process. (Worker Thread - doPropose)
        std::atomic<uint32_t>               finn_proc_sidx; // Points to finished index. (Worker Thread - doPropose)
        std::atomic<uint32_t>               prepare_sidx;   // Points to the next free index that is shown between doPropose calls
                                                            // This gives a short term to fill in the slots of the doPropose callers.
        std::atomic<int32_t>                outstanding;    // Counts the pending requests. This limits the maximum number of doPropose calls.
                                                            
        WorkerThread() : wrkt_nhdl(0), wrk_sig(0), wrkspace(new LocalSlot[MAX_NSLOTS]), 
            next_free_sidx(0), outstanding(0), finn_proc_sidx(0), prepare_sidx(0) { 
                std::memset(wrkspace, 0, sizeof(struct LocalSlot) * MAX_NSLOTS);
            }

        WorkerThread(std::thread& arg_t, std::thread::native_handle_type arg_hdl) :
            wrkt(std::move(arg_t)), wrkt_nhdl(arg_hdl), wrk_sig(0), wrkspace(new LocalSlot[MAX_NSLOTS]), 
            next_free_sidx(0), outstanding(0), finn_proc_sidx(0), prepare_sidx(0)
                { 
                    std::memset(wrkspace, 0, sizeof(struct LocalSlot) * MAX_NSLOTS);
                }

        ~WorkerThread() { delete[] wrkspace; }
    };

    int __testRdmaWrite(struct ibv_qp*, struct ibv_mr*, struct ibv_mr*, int, int);
    int __testRdmaRead(struct ibv_qp*, struct ibv_mr*, struct ibv_mr*, int, int);
}