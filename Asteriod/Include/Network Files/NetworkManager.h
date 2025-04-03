#pragma once

#include <enet/enet.h>
#include <vector>
#include <string>
#include "Player.h"
#include "GamePacket.h"

struct PlayerActionPacket {
    PacketHeader header;
    unsigned char playerID;
    unsigned char actionFlags;
    float direction;
    AEVec2 position;
    AEVec2 velocity;
};

enum ActionFlags {
    ACTION_MOVE_UP = 1 << 0,
    ACTION_MOVE_DOWN = 1 << 1,
    ACTION_MOVE_LEFT = 1 << 2,
    ACTION_MOVE_RIGHT = 1 << 3,
    ACTION_SHOOT = 1 << 4
};


class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    bool    Initialize(bool isServer);
	void    SendData(ENetPeer* peer, const char* data, int dataSize);
    int     ReceiveData(char* buffer, int bufferSize);
    bool    sendPacketAck(ENetPeer* peer, uint32_t sessionId, uint32_t offset);
    bool    sendPacketAndWaitForAck(ENetPeer* peer, const std::vector<char>& packetData, uint32_t sessionId, uint32_t offset);
    void    HandleNewConnection(ENetPeer* peer);
    void    HandlePlayerActionPacket(ENetPeer* peer, const PlayerInputPacket& packet);
    void    Run();

	bool    IsServer() const { return isServer; }
	ENetHost* GetHost() const { return enetHost; }
    void HandleAsteroidSpawnPacket(const AsteroidSpawnPacket& packet);
    void HandleAsteroidDestroyPacket(const AsteroidDestroyPacket& packet);

    // Settors
    void SetPlayerID(unsigned char id) { playerID = id; }
    void setServer(std::string ip) { serverIP = ip; }

    // Gettors
    std::string             GetServerIp() const { return serverIP;  }
    std::vector<Player>&    GetPlayers()        { return players;   }
    unsigned char           GetPlayerID() const { return playerID;  }
    ENetPeer*               GetPeer() const     { return enetPeer;  }

private:
    ENetHost* enetHost;
    ENetPeer* enetPeer;
    std::vector<Player> players;
    std::string serverIP;
    unsigned char nextPlayerID;
    unsigned char playerID;
    bool isServer;
};


enum TYPE
{
    // list of game object types
    TYPE_SHIP = 0,
    TYPE_BULLET,
    TYPE_ASTEROID,
    TYPE_WALL,

    TYPE_NUM
};

extern NetworkManager networkManager;