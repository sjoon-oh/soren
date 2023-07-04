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

#define     GET_MR_GLOBAL(NID, DIV)                 (0x2dc6c0 + (10000 * uint32_t(DIV)) + uint32_t(NID))
#define     REMOTE_REPLAYER_MR_2_LOCAL(MRID, RNID, DIV) \
                                                    (((MRID) - 0x2dc6c0) + uint32_t(RNID) + (10000 * uint32_t(DIV)))
#define     GET_QP_REPLAYER(NID, DIV)               (0x3d0900 + (10000 * uint32_t(DIV)) + uint32_t(NID))
#define     GET_SCQ_REPLAYER(NID, DIV)              (0x4c4b40 + (10000 * uint32_t(DIV)) + uint32_t(NID))
#define     GET_RCQ_REPLAYER(NID, DIV)              (0x5b8d80 + (10000 * uint32_t(DIV)) + uint32_t(NID))
#define     GET_QP_REPLICATOR(NID, RNID, DIV)       (0x6acfc0 + (10000 * uint32_t(DIV)) + (100 * uint32_t(RNID)) + uint32_t(NID))
#define     GET_SCQ_REPLICATOR(NID, RNID, DIV)      (0x7a1200 + (10000 * uint32_t(DIV)) + (100 * uint32_t(RNID)) + uint32_t(NID))
#define     GET_RCQ_REPLICATOR(NID, RNID, DIV)      (0x895440 + (10000 * uint32_t(DIV)) + (100 * uint32_t(RNID)) + uint32_t(NID))

    const size_t BUF_SIZE  = 2147483648;
    const size_t ALIGNMENT = 64;

    class Connector final {
    private:
        int             node_id;
        int             nplayers;

    public:
        Connector();
        ~Connector();

        int getNodeId();
        int getNumPlayers();
    };

    void prepareNextAlignedOffset(size_t&, size_t&, size_t);

}