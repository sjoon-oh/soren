#pragma once
/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdint>

namespace soren {

    // Memory Region Predefined Values
    enum {
        COMMON_PD,
    };

#define     GET_MR_GLOBAL(NID, SUBPAR)              (0x2dc6c0 + (10000 * uint32_t(SUBPAR)) + uint32_t(NID))
#define     GET_QP_REPLAYER(NID, SUBPAR)            (0x3d0900 + (10000 * uint32_t(SUBPAR)) + uint32_t(NID))
#define     GET_SCQ_REPLAYER(NID, SUBPAR)           (0x4c4b40 + (10000 * uint32_t(SUBPAR)) + uint32_t(NID))
#define     GET_RCQ_REPLAYER(NID, SUBPAR)           (0x5b8d80 + (10000 * uint32_t(SUBPAR)) + uint32_t(NID))
#define     GET_QP_REPLICATOR(NID, RNID, SUBPAR)    (0x6acfc0 + (10000 * uint32_t(SUBPAR)) + (100 * uint32_t(RNID)) + uint32_t(NID))
#define     GET_SCQ_REPLICATOR(NID, RNID, SUBPAR)   (0x7a1200 + (10000 * uint32_t(SUBPAR)) + (100 * uint32_t(RNID)) + uint32_t(NID))
#define     GET_RCQ_REPLICATOR(NID, RNID, SUBPAR)   (0x895440 + (10000 * uint32_t(SUBPAR)) + (100 * uint32_t(RNID)) + uint32_t(NID))

    const size_t BUF_SIZE  = 2 * 1024 * 1024;
    const size_t ALIGNMENT = 64;

    class Connector final {
    private:
        int             node_id;
        int             nplayers;

    public:
        Connector(uint32_t = 1);
        ~Connector();

        int getNodeId();
        int getNumPlayers();
    };

    void prepareNextAlignedOffset(int&, int&, int);

}