#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdint>
#include <vector>

namespace soren {

    enum {
        PLAYER_OPERATABLE = 0,
        PLAYER_UNREACHABLE
    };

    class Partitioner {
    private:
        uint16_t                players;
        uint32_t                ranger;    
        std::vector<uint32_t>   mapping{}; 

    public:
        Partitioner(uint32_t, uint32_t);
        ~Partitioner();

        void doIncrRange();
        void doDecrRange();

        bool setPlayerDead(uint32_t);
        bool setPlayerLive(uint32_t);

        uint32_t doGetOwner(char*) const;
    };
}