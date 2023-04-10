/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "soren-c.h"
#include "soren.hh"

void* cwInitConnection(uint32_t arg_subpar) {
    return reinterpret_cast<soren::Connector*>(new soren::Connector(arg_subpar));
}

void cwCleanConnection(void* arg_conn) {
    auto conn = reinterpret_cast<soren::Connector*>(arg_conn);
    delete conn;
}

int cwGetNumPlayers(void* arg_conn) {
    auto conn = reinterpret_cast<soren::Connector*>(arg_conn);
    return conn->getNodeId();
}

void* cwInitReplicator(uint32_t arg_nid, void* arg_conn) {
    auto conn = reinterpret_cast<soren::Connector*>(arg_conn);
    return reinterpret_cast<soren::Replicator*>(new soren::Replicator(arg_nid, conn->getNumPlayers(), 10, 1));
}