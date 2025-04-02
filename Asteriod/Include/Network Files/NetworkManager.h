#pragma once

#include <enet/enet.h>
#include <vector>
#include <string>
#include "Player.h"
#include "GamePacket.h"

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
    void    Run();


    //Settors
    void SetPlayerID(unsigned char id) { playerID = id; }
    void setServer(std::string ip) { serverIP = ip; }

    // Gettors
    std::string GetServerIp() const { return serverIP; }
    std::vector<Player>& GetPlayers() { return players; }
    unsigned char GetPlayerID() const { return playerID; }

private:
    ENetHost* enetHost;
    ENetPeer* enetPeer;
    std::vector<Player> players;
    std::string serverIP;
    unsigned char nextPlayerID;
    unsigned char playerID;
    bool isServer;
};

extern NetworkManager networkManager;