#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdint>

namespace soren {

    enum {
        ROLE_REPLICATOR,
        ROLE_REPLAYER
    };

    const char log_entry_prt = 0xf3;
    struct log_entry {
        char    prt;
        char    log[63];
    };

    // Simple:
    // Each role have one memory region, one queue pair. 

    class Player {
    private:
        uint32_t    node_id;
        uint8_t*    workspace;

    public:
        Player() : workspace(nullptr) { };
        Player(uint32_t arg_node_id) : node_id(arg_node_id), workspace(nullptr) { };
        virtual ~Player();

        virtual void doLaunchPlayer();
    };

    class Replicator : public Player {
    private:

    public:
        Replicator();
        ~Replicator();

        virtual void doLaunchPlayer();
    };

    //
    //
    class Replayer : public Player {
    private:

    public:

        
        virtual void doLaunchPlayer();
    };
}