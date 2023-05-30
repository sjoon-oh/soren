#pragma once
/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "timer.hh"
#include "logger.hh"    // Soren Logger

#include "hashtable.hh"

//
// This contains local Heartbeat runner.
#include "heartbeat.hh"

// Key spaces.
#include "dependency.hh"

//
// This defines the player's role: replicator and replayer. 
//  Check the document for more info.
#include "replayer.hh"
#include "replicator.hh"
#include "connector.hh"

//
// This header provides two functions to initiate and clean up Soren.
// initSoren() has basic setup process that utilizes three basic class instances: 
//  Replayer, Replicator and Connector.
// Refer to each source file for more info.
namespace soren {

    void initSoren(std::function<int(uint8_t*, size_t, int, void*)> = nullptr);
                                                    // Initialize Soren.
    void cleanSoren();                              // Clean up all the resources Soren consumed.

    Connector*  getConnector();                     // Access global Connector
    Replayer*   getReplayer();                      // Access global Replayer
    Replicator* getReplicator();                    // Access global Replicator
}