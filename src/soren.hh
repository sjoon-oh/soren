#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "timer.hh"
#include "logger.hh"    // Soren Logger

//
// This contains local Heartbeat runner.
#include "heartbeat.hh"

// Key spaces.
#include "partition.hh"

//
// This defines the player's role: replicator and replayer. 
//  Check the document for more info.
#include "player.hh"
#include "connector.hh"

namespace soren {

    void initSoren(uint32_t = 10, uint32_t = 1);
    void cleanSoren();

    Connector* getConnector();
    Replayer* getReplayer();
    Replicator* getReplicator();
}