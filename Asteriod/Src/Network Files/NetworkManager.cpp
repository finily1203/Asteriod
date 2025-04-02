#define WIN32_LEAN_AND_MEAN


#include "main.h"
#include "NetworkManager.h"
#include "GameState_Asteroids.h" +
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <cstring>   // For memcpy



std::string GetLocalIPAddress() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        return "Unknown";
    }

    struct addrinfo hints, * info, * p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(hostname, NULL, &hints, &info) != 0) {
        return "Unknown";
    }

    for (p = info; p != NULL; p = p->ai_next) {
        struct sockaddr_in* addr = (struct sockaddr_in*)p->ai_addr;
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
        freeaddrinfo(info);
        return std::string(ip);
    }

    freeaddrinfo(info);
    return "Unknown";
}


NetworkManager::NetworkManager() : enetHost(nullptr), enetPeer(nullptr), nextPlayerID(1), playerID(0), isServer(false) {
    if (enet_initialize() != 0) {
        std::cerr << "An error occurred while initializing ENet." << std::endl;
    }
}

NetworkManager::~NetworkManager() {
    if (enetHost) {
        enet_host_destroy(enetHost);
    }
    enet_deinitialize();
}

bool NetworkManager::Initialize(bool isServer) {
    this->isServer = isServer;
    if (isServer) {
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = 8888;
        enetHost = enet_host_create(&address, 32, 2, 0, 0);
        if (enetHost == nullptr) {
            std::cerr << "An error occurred while trying to create an ENet server host." << std::endl;
            return false;
        }
        // Store the server IP address
        serverIP = GetLocalIPAddress();
    }
    else {
        enetHost = enet_host_create(nullptr, 1, 2, 0, 0);
        if (enetHost == nullptr) {
            std::cerr << "An error occurred while trying to create an ENet client host." << std::endl;
            return false;
        }
        ENetAddress address;
        enet_address_set_host(&address, serverIP.c_str());
        address.port = 8888;
        enetPeer = enet_host_connect(enetHost, &address, 2, 0);
        if (enetPeer == nullptr) {
            std::cerr << "No available peers for initiating an ENet connection." << std::endl;
            return false;
        }
    }
    return true;
}


void NetworkManager::SendData(ENetPeer* peer, const char* data, int dataSize) {
    ENetPacket* packet = enet_packet_create(data, dataSize, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(enetHost);
}

int NetworkManager::ReceiveData(char* buffer, int bufferSize) {
    ENetEvent event;
    while (enet_host_service(enetHost, &event, 0) > 0) {
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            int dataLength = (bufferSize < static_cast<int>(event.packet->dataLength)) ? bufferSize : static_cast<int>(event.packet->dataLength);
            memcpy(buffer, event.packet->data, static_cast<size_t>(dataLength));
            enet_packet_destroy(event.packet);
            return event.packet->dataLength;
        }
    }
    return 0;
}

bool NetworkManager::sendPacketAck(ENetPeer* peer, uint32_t sessionId, uint32_t offset) {
    char ackBuffer[9];
    ackBuffer[0] = 0x01; // Acknowledgment flag
    uint32_t netSessionId = htonl(sessionId);
    uint32_t netOffset = htonl(offset);
    memcpy(ackBuffer + 1, &netSessionId, 4);
    memcpy(ackBuffer + 5, &netOffset, 4);

    ENetPacket* packet = enet_packet_create(ackBuffer, sizeof(ackBuffer), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(enetHost);

    return true;
}

bool NetworkManager::sendPacketAndWaitForAck(ENetPeer* peer, const std::vector<char>& packetData, uint32_t sessionId, uint32_t offset) {
    while (true) {
        ENetPacket* packet = enet_packet_create(packetData.data(), packetData.size(), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, 0, packet);
        enet_host_flush(enetHost);

        ENetEvent event;
        while (enet_host_service(enetHost, &event, 500) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                if (event.packet->dataLength == 9 && event.packet->data[0] == 0x01) {
                    uint32_t ackSessionId, ackOffset;
                    memcpy(&ackSessionId, event.packet->data + 1, 4);
                    memcpy(&ackOffset, event.packet->data + 5, 4);

                    ackSessionId = ntohl(ackSessionId);
                    ackOffset = ntohl(ackOffset);

                    if (ackSessionId == sessionId && ackOffset == offset) {
                        enet_packet_destroy(event.packet);
                        return true;  // Correct acknowledgment, proceed to next chunk
                    }
                }
                enet_packet_destroy(event.packet);
            }
        }
        std::cerr << "Timing out, retrying..." << std::endl;
    }
}

void NetworkManager::HandleNewConnection(ENetPeer* peer) {
    unsigned char playerID = nextPlayerID++;
    Player newPlayer(playerID, "Player");
    newPlayer.SetConnected(true);
    players.push_back(newPlayer);

    PlayerJoinPacket packet;
    packet.header.packetType = PT_PLAYER_JOIN;
    packet.header.sequenceNumber = 0; // Set appropriate sequence number
    packet.header.timestamp = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    packet.playerID = playerID;
    strcpy_s(packet.name, "Player"); // Set player name

    ENetPacket* enetPacket = enet_packet_create(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, enetPacket);
    enet_host_flush(enetHost);
}


void NetworkManager::HandlePlayerActionPacket(const PlayerActionPacket& packet) {
    // Find the player by ID
    for (auto& player : players) {
        if (player.GetID() == packet.playerID) {
            GameObjInst* ship = player.GetShip();
            if (ship) {
                // Update the GameObjInst state based on the received action
                ship->posCurr = packet.position;
                ship->velCurr = packet.velocity;
                ship->dirCurr = packet.direction;

                if (packet.actionFlags & ACTION_SHOOT) {
                    // Handle shooting action
                    // Create a new bullet instance
                    AEVec2 bulletDirection;
                    AEVec2Set(&bulletDirection, cosf(packet.direction), sinf(packet.direction));
                    AEVec2 bulletVelocity;
                    AEVec2Scale(&bulletVelocity, &bulletDirection, 400.0f);
                    AEVec2 bulletPosition = packet.position;
                    AEVec2 bulletScale;
                    AEVec2Set(&bulletScale, 20.0f, 3.0f);
                    gameObjInstCreate(TYPE_BULLET, &bulletScale, &bulletPosition, &bulletVelocity, packet.direction);
                }
            }
            break;
        }
    }
}

void NetworkManager::Run() {
    ENetEvent event;
    while (g_isRunning) { // Check the global running flag
        while (enet_host_service(enetHost, &event, 1000) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                std::cout << "A new client connected from " << event.peer->address.host << ":" << event.peer->address.port << std::endl;
                // Handle new client connection if server
                if (isServer) {
                    HandleNewConnection(event.peer);
                }
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                std::cout << "A packet of length " << event.packet->dataLength << " was received." << std::endl;
                // Handle received data
                if (event.packet->dataLength == sizeof(PlayerActionPacket)) {
                    PlayerActionPacket* packet = reinterpret_cast<PlayerActionPacket*>(event.packet->data);
                    if (packet->header.packetType == PT_PLAYER_ACTION) {
                        HandlePlayerActionPacket(*packet);
                    }
                }
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                std::cout << "Client disconnected." << std::endl;
                event.peer->data = nullptr;
                break;
            default:
                break;
            }
        }
    }
}