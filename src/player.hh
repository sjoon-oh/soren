#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */
#include <stddef.h>
#include <cstdint>
#include <cstdlib>

#include <mutex>
#include <array>
#include <vector>
#include <map>      

#include <atomic>
#include <thread>

// Resource Representation
#include "partition.hh"
#include "connector.hh"

#include "commons.hh"

#include <infiniband/verbs.h>

// #include <infiniband/verbs.h> // OFED IB verbs

namespace soren {

    const int MAX_NWORKER   = 8;
    const int MAX_SUBPAR    = 8;

    const int MAX_REPLICATOR_WORKER = MAX_NWORKER / 2;
    const int MAX_REPLAYER_WORKER = MAX_NWORKER;

    const int MAX_NSLOTS    = 256;

    enum {
        SIG_PAUSE       = 0x00,
        SIG_SELFRET    = 0x02,
        SIG_CONT,
        SIG_PROPOSE     = 0xff,
        SIG_WORKEND,
        SIG_READY,
        SIG_NOTIFIED,
    };

    struct log_entry {
        char*       addr;
        uint32_t    size;
    };

    struct WorkerThread {
        std::thread                         wrkt;           // Thread obj
        std::thread::native_handle_type     wrkt_nhdl;      // Native handle.
        std::atomic<int32_t>                wrk_sig;        // Signal Var.

        Slot*                               wrkspace;       // Per-thread workspace.
        std::atomic<u_char>                 next_free_sidx; // Next to process.
        std::atomic<u_char>                 finn_proc_sidx;
        std::atomic<int32_t>                outstanding;
                                                            // Records the latest processed slot idx.

        WorkerThread() : wrkt_nhdl(0), wrk_sig(0), wrkspace(new Slot[MAX_NSLOTS]), 
            next_free_sidx(1), outstanding(0), finn_proc_sidx(128) { }
        WorkerThread(std::thread& arg_t, std::thread::native_handle_type arg_hdl) :
            wrkt(std::move(arg_t)), wrkt_nhdl(arg_hdl), wrk_sig(0), wrkspace(new Slot[MAX_NSLOTS]), 
            next_free_sidx(1), outstanding(0), finn_proc_sidx(128)
                { }

        ~WorkerThread() { delete[] wrkspace; }
    };

    int __testRdmaWrite(struct ibv_qp*, struct ibv_mr*, struct ibv_mr*, int, int);
    int __testRdmaRead(struct ibv_qp*, struct ibv_mr*, struct ibv_mr*, int, int);
}