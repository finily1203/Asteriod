#pragma once

#include <WinSock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <cstring> 
#include "Player.h"

// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    bool Initialize(bool isServer, const char* serverIP = nullptr);
    void Run();
    void AddPlayer(unsigned char id, const char* name);

private:
    bool InitializeWinsock();
    SOCKET CreateUDPSocket();
    bool BindSocket(SOCKET udpSocket, int port);
    void SendData(SOCKET udpSocket, const char* data, int dataSize, const sockaddr_in& clientAddr);
    int ReceiveData(SOCKET udpSocket, char* buffer, int bufferSize, sockaddr_in& clientAddr);

    SOCKET udpSocket;
    std::vector<Player> players;
};