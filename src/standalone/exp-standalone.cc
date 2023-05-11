/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>
#include <random>

#include <iostream>
#include <iomanip>

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

//
// Main. 
int main(int argc, char *argv[]) {

    const int MAX_PAYLOAD_SZ = 512;

    const int PAYLOAD_SZ = atoi(argv[1]);
    const int KEY_SZ = atoi(argv[2]);

    const size_t DEFAULT_BUFSZ = 2147483648;
    
    size_t NUM_REQUESTS = DEFAULT_BUFSZ / (120 + PAYLOAD_SZ); // Give some space
    // NUM_REQUESTS = 10000; 

    std::cout << "Requests: " << NUM_REQUESTS << std::endl;
    // const int NUM_REQUESTS = 4096;

    struct Payload {
        char buffer[MAX_PAYLOAD_SZ];
    } local_buffer;

    //
    // Initialize randomization device
    std::random_device random_device;
    std::mt19937 generator(random_device());

    soren::initTimestamps(NUM_REQUESTS);
    soren::initSoren(10, 1);

    std::cout << "Standalone test start.\n";

    for (size_t nth_req = 0; nth_req < NUM_REQUESTS; nth_req++) {

        // Randomize the payload.
        soren::generateRandomStr(generator, local_buffer.buffer, PAYLOAD_SZ);
        local_buffer.buffer[PAYLOAD_SZ - 1] = 0;

        soren::__MARK_TS_BEFORE__();
        soren::getReplicator()->doPropose(
            reinterpret_cast<uint8_t*>(local_buffer.buffer), PAYLOAD_SZ,
            reinterpret_cast<uint8_t*>(local_buffer.buffer), KEY_SZ,
            soren::REQTYPE_REPLICATE
        );
        soren::__MARK_TS_AFTER__(nth_req);

        if (nth_req % 10000 == 0)
            std::cout << "\rIteration: " << nth_req << std::flush;
    }

    std::cout << "\n";

    for (int sec = 300; sec > 0; sec--) {
        std::cout <<"\rWaiting for: " << sec << " sec     " << std::flush;
        sleep(1);
    }

    std::cout <<"\nExporting...\n";

    soren::dumpElapsedTimes();
    soren::cleanSoren();

    std::cout <<"Exit.\n";
    
    return 0;
}

