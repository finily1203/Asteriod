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

/******************************************************************************/
/*!
    Defines
*/
/******************************************************************************/
const unsigned int	GAME_OBJ_NUM_MAX = 32;			// The total number of different objects (Shapes)
const unsigned int	GAME_OBJ_INST_NUM_MAX = 2048;			// The total number of different game object instances


const unsigned int	SHIP_INITIAL_NUM = 3;			// initial number of ship lives
const float			SHIP_SCALE_X = 16.0f;		// ship scale x
const float			SHIP_SCALE_Y = 16.0f;		// ship scale y
const float			BULLET_SCALE_X = 20.0f;		// bullet scale x
const float			BULLET_SCALE_Y = 3.0f;			// bullet scale y
const float			ASTEROID_MIN_SCALE_X = 10.0f;		// asteroid minimum scale x
const float			ASTEROID_MAX_SCALE_X = 60.0f;		// asteroid maximum scale x
const float			ASTEROID_MIN_SCALE_Y = 10.0f;		// asteroid minimum scale y
const float			ASTEROID_MAX_SCALE_Y = 60.0f;		// asteroid maximum scale y

const float			WALL_SCALE_X = 64.0f;		// wall scale x
const float			WALL_SCALE_Y = 164.0f;		// wall scale y

const float			SHIP_VELOCITY_CAP = 0.99f;		// ship velocity cap
const float			SHIP_ACCEL_FORWARD = 100.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_ACCEL_BACKWARD = 100.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_ROT_SPEED = (2.0f * PI);	// ship rotation speed (degree/second)

const float			BULLET_SPEED = 400.0f;		// bullet speed (m/s)

const float         BOUNDING_RECT_SIZE = 1.0f;         // this is the normalized bounding rectangle (width and height) sizes - AABB collision data

// -----------------------------------------------------------------------------
enum TYPE
{
    // list of game object types
    TYPE_SHIP = 0,
    TYPE_BULLET,
    TYPE_ASTEROID,
    TYPE_WALL,

    TYPE_NUM
};

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

void NetworkManager::Run() {
    ENetEvent event;
    while (g_isRunning) { // Check the global running flag
        while (enet_host_service(enetHost, &event, 1000) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                std::cout << "A new client connected from " << event.peer->address.host << ":" << event.peer->address.port << std::endl;
                // Handle new client connection if server
                event.peer->data = reinterpret_cast<void*>(nextPlayerID);
                nextPlayerID++;
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                std::cout << "A packet of length " << event.packet->dataLength << " was received." << std::endl;
                // Handle received data
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
        if (isServer) {

        }
    }
}