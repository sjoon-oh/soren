/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <iostream>

#include <cstring>
#include <algorithm>

#include "partition.hh"

soren::Partitioner::Partitioner(uint32_t arg_np, uint32_t arg_ranger = 10) : players(arg_np) {
    for (int plyr = 0; plyr < arg_np; plyr++)
        mapping.push_back(plyr);
    arg_ranger = 0;
}
soren::Partitioner::~Partitioner() {};

void soren::Partitioner::doIncrRange() { ranger++; }
void soren::Partitioner::doDecrRange() { 
    if (ranger == 0) return;
    ranger--;
}

bool soren::Partitioner::setPlayerDead(uint32_t arg_node_id) {
    auto itor = std::find(mapping.begin(), mapping.end(), arg_node_id);
    
    if (itor == mapping.end()) 
        return false; // Something is wrong.
    
    mapping.erase(itor);
    players = mapping.size();

    return true;
}

bool soren::Partitioner::setPlayerLive(uint32_t arg_node_id) {
    auto itor = std::find(mapping.begin(), mapping.end(), arg_node_id);
    
    if (itor != mapping.end()) 
        return false; // Something is wrong.
    
    mapping.push_back(arg_node_id);
    players = mapping.size();

    return true;
}

uint32_t soren::Partitioner::doGetOwner(char* arg_tar) const {
    uint32_t tar = 0;
    std::memcpy(&tar, arg_tar, sizeof(uint32_t));

    tar = (tar >> ranger) % players;
    
    return mapping.at(tar);
};