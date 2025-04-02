#ifndef GAME_PACKET_H
#define GAME_PACKET_H

#include "AEEngine.h"
#include "Collision.h" // For AABB

#pragma pack(push, 1) // Ensure tight packing for network transmission

// Base packet structure
struct PacketHeader {
    unsigned short packetType;   // Identifies packet type
    unsigned int sequenceNumber; // For ordering and ACKs
    float timestamp;             // Time packet was sent
};

// Packet types
enum PacketType {
    PT_INVALID = 0,
    PT_PLAYER_INPUT,         // Client -> Server: Player controls
    PT_GAME_STATE,           // Server -> Client: Full game state
    PT_ASTEROID_SPAWN,       // Server -> Client: New asteroid
    PT_ASTEROID_DESTROY,     // Server -> Client: Asteroid destroyed
    PT_PLAYER_HIT,           // Server -> Client: Player was hit
    PT_SCORE_UPDATE,         // Server -> Client: Score change
    PT_GAME_OVER,            // Server -> Client: Game ended
    PT_PLAYER_JOIN,          // Both directions
    PT_PLAYER_LEAVE,         // Server -> Client
    PT_ACK,                  // Acknowledgment
    PT_PLAYER_ACTION         // Client -> Server: Player action
};

// Player input packet (Client -> Server)
struct PlayerInputPacket {
    PacketHeader header;
    unsigned char playerID;
    unsigned char inputFlags;    // Bitmask of InputFlags
    AEVec2 position;
    AEVec2 velocity;
    float direction;
};

// Game state packet (Server -> Client)
struct GameStatePacket {
    PacketHeader header;

    struct ObjectState {
        unsigned short objectID;
        unsigned char type;      // TYPE_SHIP, TYPE_BULLET, etc.
        AEVec2 position;
        AEVec2 velocity;
        float rotation;
        AABB boundingBox;
    };

    unsigned char playerCount;
    struct PlayerData {
        unsigned char playerID;
        char name[32];
        int score;
        bool isAlive;
    } players[4];

    unsigned short objectCount;
    ObjectState objects[50]; // Adjust size as needed
};

// Asteroid spawn packet (Server -> Client)
struct AsteroidSpawnPacket {
    PacketHeader header;
    unsigned short asteroidID;
    AEVec2 position;
    AEVec2 velocity;
    AEVec2 scale;
};

// Asteroid destroy packet (Server -> Client)
struct AsteroidDestroyPacket {
    PacketHeader header;
    unsigned short asteroidID;
    unsigned char destroyedBy; // Player ID who destroyed it
};

// Score update packet (Server -> Client)
struct ScoreUpdatePacket {
    PacketHeader header;
    unsigned char playerID;
    int scoreDelta;
};

// Game over packet (Server -> Client)
struct GameOverPacket {
    PacketHeader header;
    unsigned char winningPlayerID;
    int scores[4]; // Final scores for all players
};

// Player join/leave packets
struct PlayerJoinPacket {
    PacketHeader header;
    unsigned char playerID;
    char name[32];
};

struct PlayerLeavePacket {
    PacketHeader header;
    unsigned char playerID;
};

// Acknowledgment packet
struct AckPacket {
    PacketHeader header;
    unsigned int ackedSequenceNumber;
};

#pragma pack(pop) // Restore default packing

#endif // GAME_PACKET_H