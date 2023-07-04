/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>
#include <random>

#include <iostream>
#include <iomanip>

#include <thread>

#include "../soren.hh"

// #include <thread>
// #include <functional>

// Forward Declaration.

namespace soren {
    void generateRandomStr(std::mt19937& arg_gen, char* arg_buf, int arg_len) {

        // Printable ASCII range.
        const char* character_set = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        
        std::uniform_int_distribution<int> distributor(0, 61);
        for (int i = 0; i < arg_len; i++)
            arg_buf[i] = character_set[static_cast<char>(distributor(arg_gen))];
    }
}

int dummy_replayf(uint8_t* arg_memaddr, size_t arg_memsz, int argc, void* argv) {

    std::cout << "Replay function called, printing args:\n";
    std::cout << "Addr: " << arg_memaddr << ", Size: " << arg_memsz << ", Aux#: " << argc << std::endl;

    return 0;
}

const int MAX_PAYLOAD_SZ = 512;
struct Payload {
    char buffer[MAX_PAYLOAD_SZ];
};

//
// Main. 
int main(int argc, char *argv[]) {

    const int PAYLOAD_SZ = atoi(argv[1]);
    const int KEY_SZ = atoi(argv[2]);

    const int MAX_THREADS = 4;

    const size_t DEFAULT_BUFSZ = 2147483648;
    
    size_t NUM_REQUESTS = DEFAULT_BUFSZ / (120 + PAYLOAD_SZ); // Give some space
    size_t nreq_for_thread = NUM_REQUESTS / MAX_THREADS;
    // NUM_REQUESTS = 10000; 

    std::cout << "Requests: " << NUM_REQUESTS << std::endl;
    // const int NUM_REQUESTS = 4096;

    soren::initTimestamps(NUM_REQUESTS);
    soren::initSoren(nullptr);

    std::cout << "Standalone test start.\n";

    // std::thread wrkr_threads;
    std::vector<std::thread> wrkr_threads;

    for (int i = 0; i < MAX_THREADS; i++) {

        std::thread wrkr(
            [](int thread_id, size_t nreq_for_thread, int payload_sz, int key_sz) {

                //
                // Initialize randomization device
                std::random_device random_device;
                std::mt19937 generator(random_device());

                Payload local_buffer;
                int32_t idx;

                std::cout <<"TID " << thread_id << " >> Expected to have " << nreq_for_thread << " requests." << std::endl;

                for (size_t nth_req = 0; nth_req < nreq_for_thread; nth_req++) {

                    soren::generateRandomStr(generator, local_buffer.buffer, payload_sz);
                    local_buffer.buffer[payload_sz - 1] = 0;

                    idx = soren::__MARK_TS_BEFORE__();
                    soren::getReplicator()->doPropose(
                        reinterpret_cast<uint8_t*>(local_buffer.buffer), payload_sz,
                        reinterpret_cast<uint8_t*>(local_buffer.buffer), key_sz,
                        0, soren::REQTYPE_REPLICATE
                    );
                    soren::__MARK_TS_AFTER__(idx);

                    if (nth_req % 50000 == 0)
                        std::cout <<"TID " << thread_id << " >> Latest cnt: " << nth_req << std::endl;
                }

                std::cout <<"TID " << thread_id << " >> Returning." << std::endl;
            },
            i, nreq_for_thread, PAYLOAD_SZ, KEY_SZ
        );
        wrkr_threads.push_back(std::move(wrkr));
    }

    for (auto& w: wrkr_threads)
        w.join();

    for (int sec = 150; sec > 0; sec--) {
        std::cout <<"\rWaiting for: " << sec << " sec     " << std::flush;
        sleep(1);
    }

    std::cout <<"\nExporting...\n";

    soren::dumpElapsedTimes();
    soren::cleanSoren();

    std::cout <<"Exit.\n";
    
    return 0;
}

