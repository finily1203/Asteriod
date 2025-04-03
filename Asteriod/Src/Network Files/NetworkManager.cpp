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
    newPlayer.SetPeer(peer); // Set the peer for the new player
    players.push_back(newPlayer);

    // Create a new player instance in the game state
    AEVec2 scale;
    AEVec2Set(&scale, 16.0f, 16.0f);
    GameObjInst* newPlayerShip = gameObjInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
    AE_ASSERT(newPlayerShip);

    // Assign the ship to the new player
    newPlayer.SetShip(newPlayerShip);

    // Send a packet to the new player to inform them of their player ID
    PlayerJoinPacket joinPacket;
    joinPacket.header.packetType = PT_PLAYER_JOIN;
    joinPacket.header.sequenceNumber = 0; // Set appropriate sequence number
    joinPacket.header.timestamp = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    joinPacket.playerID = playerID;
    strcpy_s(joinPacket.name, "Player"); // Set player name

    ENetPacket* enetJoinPacket = enet_packet_create(&joinPacket, sizeof(joinPacket), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, enetJoinPacket);
    enet_host_flush(enetHost);

    // Send the updated player list to all clients
    GameStatePacket gameStatePacket;
    gameStatePacket.header.packetType = PT_GAME_STATE;
    gameStatePacket.header.sequenceNumber = 0; // Set appropriate sequence number
    gameStatePacket.header.timestamp = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    // Populate player data
    gameStatePacket.playerCount = static_cast<unsigned char>(players.size());
    for (size_t i = 0; i < players.size(); ++i) {
        gameStatePacket.players[i].playerID = players[i].GetID();
        strcpy_s(gameStatePacket.players[i].name, players[i].GetName().c_str());
        gameStatePacket.players[i].score = 0; // Set appropriate score
        gameStatePacket.players[i].isAlive = (players[i].GetShip() != nullptr);
    }

    ENetPacket* enetGameStatePacket = enet_packet_create(&gameStatePacket, sizeof(gameStatePacket), ENET_PACKET_FLAG_RELIABLE);
    for (auto& p : players) {
        enet_peer_send(peer, 0, enetGameStatePacket); // Use the GetPeer method
    }
    enet_host_flush(enetHost);
}

void NetworkManager::HandlePlayerActionPacket(ENetPeer* peer, const PlayerInputPacket& packet) {
    // Find the player by ID
    for (auto& player : players) {
        if (player.GetID() == packet.playerID) {
            GameObjInst* ship = player.GetShip();
            if (ship) {
                // Update the GameObjInst state based on the received action
                if (packet.inputFlags & ACTION_MOVE_UP) {
                    AEVec2 added;
                    AEVec2Set(&added, cosf(ship->dirCurr) * 100.0f, sinf(ship->dirCurr) * 100.0f);
                    AEVec2Scale(&added, &added, g_dt);
                    AEVec2Add(&ship->velCurr, &ship->velCurr, &added);
                    AEVec2Scale(&ship->velCurr, &ship->velCurr, 0.99f);
                }

                if (packet.inputFlags & ACTION_MOVE_DOWN) {
                    AEVec2 added;
                    AEVec2Set(&added, -cosf(ship->dirCurr) * 100.0f, -sinf(ship->dirCurr) * 100.0f);
                    AEVec2Scale(&added, &added, g_dt);
                    AEVec2Add(&ship->velCurr, &ship->velCurr, &added);
                    AEVec2Scale(&ship->velCurr, &ship->velCurr, 0.99f);
                }

                if (packet.inputFlags & ACTION_MOVE_LEFT) {
                    ship->dirCurr += (2.0f * PI) * g_dt;
                    ship->dirCurr = AEWrap(ship->dirCurr, -PI, PI);
                }

                if (packet.inputFlags & ACTION_MOVE_RIGHT) {
                    ship->dirCurr -= (2.0f * PI) * g_dt;
                    ship->dirCurr = AEWrap(ship->dirCurr, -PI, PI);
                }
                // Update position
                ship->posCurr = packet.position;
                ship->velCurr = packet.velocity;
                ship->dirCurr = packet.direction;

                if (packet.inputFlags & ACTION_SHOOT) {
                    AEVec2 bulletDirection;
                    AEVec2Set(&bulletDirection, cosf(ship->dirCurr), sinf(ship->dirCurr));
                    AEVec2 bulletVelocity;
                    AEVec2Scale(&bulletVelocity, &bulletDirection, 400.0f);
                    AEVec2 bulletPosition = ship->posCurr;
                    AEVec2 bulletScale;
                    AEVec2Set(&bulletScale, 20.0f, 3.0f);
                    gameObjInstCreate(TYPE_BULLET, &bulletScale, &bulletPosition, &bulletVelocity, ship->dirCurr);
                }

                // Update the ship's position
                AEVec2 displacement;
                AEVec2Scale(&displacement, &ship->velCurr, g_dt);
                AEVec2Add(&ship->posCurr, &ship->posCurr, &displacement);

                // Send back the updated game state to the client
                GameStatePacket gameStatePacket;
                gameStatePacket.header.packetType = PT_GAME_STATE;
                gameStatePacket.header.sequenceNumber = 0; // Set appropriate sequence number
                gameStatePacket.header.timestamp = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

                // Populate the ObjectState for the player's ship
                GameStatePacket::ObjectState objectState;
                objectState.objectID = player.GetID();
                objectState.type = TYPE_SHIP;
                objectState.position = ship->posCurr;
                objectState.velocity = ship->velCurr;
                objectState.rotation = ship->dirCurr;
                objectState.boundingBox = ship->boundingBox;

                // Add the ObjectState to the GameStatePacket
                gameStatePacket.objectCount = 1;
                gameStatePacket.objects[0] = objectState;

                // Populate player data
                gameStatePacket.playerCount = static_cast<unsigned char>(players.size());
                for (size_t i = 0; i < players.size(); ++i) {
                    gameStatePacket.players[i].playerID = players[i].GetID();
                    strcpy_s(gameStatePacket.players[i].name, players[i].GetName().c_str());
                    gameStatePacket.players[i].score = 0; // Set appropriate score
                    gameStatePacket.players[i].isAlive = (players[i].GetShip() != nullptr);
                }

                ENetPacket* enetPacket = enet_packet_create(&gameStatePacket, sizeof(gameStatePacket), ENET_PACKET_FLAG_RELIABLE);
                if (peer != nullptr) {
                    enet_peer_send(peer, 0, enetPacket);
                }
                else {
                    // Handle the error or log it
                    std::cerr << "peer is NULL" << std::endl;
                }
                enet_host_flush(enetHost);
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

                // Check if we have at least enough data for the header
                if (event.packet->dataLength >= sizeof(PacketHeader)) {
                    PacketHeader* header = reinterpret_cast<PacketHeader*>(event.packet->data);

                    // Handle different packet types based on the header's packet type
                    switch (header->packetType) {
                    case PT_PLAYER_ACTION:
                        if (event.packet->dataLength == sizeof(PlayerInputPacket)) {
                            PlayerInputPacket* packet = reinterpret_cast<PlayerInputPacket*>(event.packet->data);
                            HandlePlayerActionPacket(event.peer, *packet);
                        }
                        break;

                        // Inside NetworkManager::Run() switch statement for packet types
                    case PT_ASTEROID_SPAWN:
                        if (event.packet->dataLength == sizeof(AsteroidSpawnPacket)) {
                            AsteroidSpawnPacket* packet = reinterpret_cast<AsteroidSpawnPacket*>(event.packet->data);
                            HandleAsteroidSpawnPacket(*packet);
                            std::cout << "Received and processed asteroid spawn packet" << std::endl;
                        }
                        break;

                    case PT_ASTEROID_DESTROY:
                        if (event.packet->dataLength == sizeof(AsteroidDestroyPacket)) {
                            AsteroidDestroyPacket* packet = reinterpret_cast<AsteroidDestroyPacket*>(event.packet->data);
                            HandleAsteroidDestroyPacket(*packet);
                        }
                        break;

                    default:
                        std::cout << "Received packet with unknown type: " << header->packetType << std::endl;
                        break;
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

void NetworkManager::HandleAsteroidSpawnPacket(const AsteroidSpawnPacket& packet) {
    if (isServer) return; // Server doesn't need to handle this

    // Create asteroid using the dedicated function
    GameObjInst* asteroid = CreateAsteroidFromPacket(packet.position, packet.velocity, packet.scale);

    if (asteroid) {
        std::cout << "Created asteroid from server data" << std::endl;
    }
}


void NetworkManager::HandleAsteroidDestroyPacket(const AsteroidDestroyPacket& packet) {
    if (isServer) return; // Server doesn't need to handle this

    // Call the dedicated function to destroy the asteroid
    DestroyAsteroidById(packet.asteroidID);
}