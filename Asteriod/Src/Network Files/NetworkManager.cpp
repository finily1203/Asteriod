#include "NetworkManager.h"
#include <iostream>

NetworkManager::NetworkManager() : udpSocket(INVALID_SOCKET) {}

NetworkManager::~NetworkManager() {
    if (udpSocket != INVALID_SOCKET) {
        closesocket(udpSocket);
        WSACleanup();
    }
}

bool NetworkManager::Initialize(bool isServer, const char* serverIP) {
    if (!InitializeWinsock()) return false;
    udpSocket = CreateUDPSocket();
    if (udpSocket == INVALID_SOCKET) return false;
    if (isServer) {
        return BindSocket(udpSocket, 8888);
    }
    else {
        // Client-specific initialization (e.g., connecting to server)
        // ...
    }
    return true;
}

void NetworkManager::Run() {
    char buffer[512];
    sockaddr_in clientAddr;
    while (true) {
        int bytesReceived = ReceiveData(udpSocket, buffer, sizeof(buffer), clientAddr);
        if (bytesReceived > 0) {
            // Handle received data
            std::cout << "Received data from client" << std::endl;
        }
    }
}

void NetworkManager::AddPlayer(unsigned char id, const char* name) {
    players.emplace_back(id, name);
}

bool NetworkManager::InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }
    return true;
}

SOCKET NetworkManager::CreateUDPSocket() {
    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return INVALID_SOCKET;
    }
    return udpSocket;
}

bool NetworkManager::BindSocket(SOCKET udpSocket, int port) {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    int result = bind(udpSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(udpSocket);
        WSACleanup();
        return false;
    }
    return true;
}

void NetworkManager::SendData(SOCKET udpSocket, const char* data, int dataSize, const sockaddr_in& clientAddr) {
    sendto(udpSocket, data, dataSize, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
}

int NetworkManager::ReceiveData(SOCKET udpSocket, char* buffer, int bufferSize, sockaddr_in& clientAddr) {
    int clientAddrSize = sizeof(clientAddr);
    return recvfrom(udpSocket, buffer, bufferSize, 0, (sockaddr*)&clientAddr, &clientAddrSize);
}