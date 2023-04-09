#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <cstdint>

namespace soren {

    // Memory Region Predefined Values
    enum {
        COMMON_PD,
    };

// #define     __PD_BASE(NODE_ID, PD_ID)            (100 * (PD_ID) + (NODE_ID))
// #define     __MR_BASE(NODE_ID, PD_ID, MR_ID)     (10000 * (MR_ID) + 100 * (PD_ID) + (NODE_ID))
// #define     __QP_BASE(NODE_ID, PD_ID, QP_ID)     (10000 * (QP_ID) + 100 * (PD_ID) + (NODE_ID))

#define     MRID(NID, SUBPAR)                   (0x2dc6c0 + (10000 * (SUBPAR)) + (NID))
#define     QPID_REPLAYER(NID, SUBPAR)          (0x3d0900 + (10000 * (SUBPAR)) + (NID))
#define     SCQID_REPLAYER(NID, SUBPAR)         (0x4c4b40 + (10000 * (SUBPAR)) + (NID))
#define     RCQID_REPLAYER(NID, SUBPAR)         (0x5b8d80 + (10000 * (SUBPAR)) + (NID))
#define     QPID_REPLICATOR(NID, RNID, SUBPAR)  (0x6acfc0 + (10000 * (SUBPAR)) + (100 * (RNID)) + (NID))
#define     SCQID_REPLICATOR(NID, RNID, SUBPAR) (0x7a1200 + (10000 * (SUBPAR)) + (100 * (RNID)) + (NID))
#define     RCQID_REPLICATOR(NID, RNID, SUBPAR) (0x895440 + (10000 * (SUBPAR)) + (100 * (RNID)) + (NID))


    class Connector final {
    private:
        int             node_id;

    public:
        Connector(uint32_t = 1);
        ~Connector();

        int getNodeId();
    };
}