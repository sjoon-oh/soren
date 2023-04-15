/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

// #include <memory>
#include <string>

#include "logger.hh"
#include "player.hh"

#include "commons.hh"
#include "connector.hh"
#include "hartebeest-wrapper.hh"

#include <infiniband/verbs.h>

namespace soren {
    static Logger RMDA_TEST_LOGGER("SOREN/RMDA-TEST", "soren_rdma_test.log");
}



/// @brief Tests RDMA WRITE.
/// @param arg_loc_qp 
/// @param arg_loc_mr 
/// @param arg_remote_mr 
/// @param arg_nid 
/// @param arg_testmode 
/// @return 
int soren::__testRdmaWrite(
    struct ibv_qp* arg_loc_qp,
    struct ibv_mr* arg_loc_mr,
    struct ibv_mr* arg_remote_mr,
    int arg_nid,
    int arg_testmode
    ) {

        SOREN_LOGGER_INFO(RMDA_TEST_LOGGER, " -- TEST RDMA WRITE -- ");

        int ret = 0;
        if (arg_testmode == 0) {

            std::string sample_str = "This is a message from node " + std::to_string(arg_nid);
            SOREN_LOGGER_INFO(RMDA_TEST_LOGGER, "SEND TO REMOTE [{}]: \"{}\"", 
                arg_remote_mr->addr, sample_str);
            
            std::memcpy(arg_loc_mr->addr, sample_str.c_str(), sample_str.size());

            ret = rdmaPost(
                IBV_WR_RDMA_WRITE,
                arg_loc_qp, 
                reinterpret_cast<uintptr_t>(arg_loc_mr->addr),
                sample_str.size(),
                arg_loc_mr->lkey,
                reinterpret_cast<uintptr_t>(arg_remote_mr->addr),
                arg_remote_mr->rkey
            );

        } else {

            SOREN_LOGGER_INFO(RMDA_TEST_LOGGER, "OBSERVING LOCAL [{}]", arg_loc_mr->addr);

            std::string sample_str;
            while (1) {
                sample_str = (char*)arg_loc_mr->addr;
                sleep(1);

                SOREN_LOGGER_INFO(RMDA_TEST_LOGGER, "MEMCHECK: {}", sample_str);
            }
        }

        SOREN_LOGGER_INFO(RMDA_TEST_LOGGER, " ----- TEST END ({}) ----- ", ret);
        return ret;
}



int soren::__testRdmaRead(
    struct ibv_qp* arg_loc_qp,
    struct ibv_mr* arg_loc_mr,
    struct ibv_mr* arg_remote_mr,
    int arg_nid,
    int arg_testmode
) {


    return 0;
}