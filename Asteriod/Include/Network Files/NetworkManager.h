#pragma once

#include <WinSock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>
#include "Player.h"
#include "GamePacket.h"

// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    bool Initialize(bool isServer);
    void Run();
    void AddPlayer(unsigned char id, const char* name);
    void SendPlayerInput(unsigned char playerID, unsigned char inputFlags);
    void ReceiveGameState();
    void HandleAcknowledgments();
    void HandleNewConnection(const sockaddr_in& clientAddr);
    void HandlePlayerJoin(const PlayerJoinPacket& packet);
    void UpdateGameState();
    void ProcessPlayerInput(unsigned char playerID, unsigned char inputFlags);

    void SetPlayerID(unsigned char id) { playerID = id; }
    unsigned char GetPlayerID() const { return playerID; }
    sockaddr_in GetServerAddress() const { return serverAddress; }
	std::string GetServerIp() const { return serverIP; }
	void setServer(std::string ip) { serverIP = ip; }
    std::vector<Player>& GetPlayers() { return players; }

private:
    bool InitializeWinsock();
    SOCKET CreateUDPSocket();
    bool BindSocket(SOCKET udpSocket, int port);
    void SendData(SOCKET udpSocket, const char* data, int dataSize, const sockaddr_in& clientAddr);
    int ReceiveData(SOCKET udpSocket, char* buffer, int bufferSize, sockaddr_in& clientAddr);
    bool sendPacketAck(SOCKET udpSocket, const sockaddr_in& clientAddr, uint32_t sessionId, uint32_t offset);
    bool sendPacketAndWaitForAck(SOCKET udpSocket, const std::vector<char>& packet, sockaddr_in& clientAddr, uint32_t sessionId, uint32_t offset);
    std::string getLocalIP();

    SOCKET udpSocket;
    std::vector<Player> players;
    sockaddr_in serverAddress;
    std::string serverIP;
    unsigned char nextPlayerID;
    unsigned char playerID;
    bool isServer;
};

extern NetworkManager networkManager;
