#include "NetworkManager.h"
#include "main.h" // Include the header where g_dt and g_appTime are defined
#include "GameState_Asteroids.h" // Include the game state header
#include <iostream>
#include <algorithm> // For std::copy

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

NetworkManager::NetworkManager() : udpSocket(INVALID_SOCKET), nextPlayerID(1), playerID(0), isServer(false) {}

NetworkManager::~NetworkManager() {
    if (udpSocket != INVALID_SOCKET) {
        closesocket(udpSocket);
        WSACleanup();
    }
}

bool NetworkManager::Initialize(bool isServer) {
    this->isServer = isServer;
    if (!InitializeWinsock()) return false;
    udpSocket = CreateUDPSocket();
    if (udpSocket == INVALID_SOCKET) return false;
    if (isServer) {
        if (!BindSocket(udpSocket, 8888)) return false;
        this->serverIP = getLocalIP();
    }
    else {
        // Client-specific initialization (e.g., connecting to server)
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(8888);
        inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
        serverAddress = serverAddr;
    }
    return true;
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
        WSACleanup();
        std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
        return INVALID_SOCKET;
    }
    return udpSocket;
}

bool NetworkManager::BindSocket(SOCKET udpSocket, int port) {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    int result = bind(udpSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(udpSocket);
        WSACleanup();
        return false;
    }
    return true;
}

std::string NetworkManager::getLocalIP() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        std::cerr << "Error getting hostname: " << WSAGetLastError() << std::endl;
        return "Unknown";
    }

    addrinfo hints{}, * info = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(hostname, nullptr, &hints, &info) != 0) {
        std::cerr << "Error getting address info: " << WSAGetLastError() << std::endl;
        return "Unknown";
    }

    std::string ipAddress = "Unknown";
    for (addrinfo* p = info; p != nullptr; p = p->ai_next) {
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(p->ai_addr);
        char ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) != nullptr) {
            ipAddress = ip;
            break;
        }
    }

    freeaddrinfo(info);
    return ipAddress;
}

void NetworkManager::SendData(SOCKET udpSocket, const char* data, int dataSize, const sockaddr_in& clientAddr) {
    sendto(udpSocket, data, dataSize, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
}

int NetworkManager::ReceiveData(SOCKET udpSocket, char* buffer, int bufferSize, sockaddr_in& clientAddr) {
    int clientAddrSize = sizeof(clientAddr);
    return recvfrom(udpSocket, buffer, bufferSize, 0, (sockaddr*)&clientAddr, &clientAddrSize);
}

bool NetworkManager::sendPacketAck(SOCKET udpSocket, const sockaddr_in& clientAddr, uint32_t sessionId, uint32_t offset) {
    char ackBuffer[9];
    ackBuffer[0] = 0x01; // Acknowledgment flag
    uint32_t netSessionId = htonl(sessionId);
    uint32_t netOffset = htonl(offset);
    memcpy(ackBuffer + 1, &netSessionId, 4);
    memcpy(ackBuffer + 5, &netOffset, 4);

    int sendResult = sendto(udpSocket, ackBuffer, sizeof(ackBuffer), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
    return sendResult != SOCKET_ERROR;
}

void NetworkManager::SendPlayerInput(unsigned char playerID, unsigned char inputFlags) {
    PlayerInputPacket packet;
    packet.header.packetType = PT_PLAYER_INPUT;
    packet.header.sequenceNumber = 0; // Set appropriate sequence number
    packet.header.timestamp = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    packet.playerID = playerID;
    packet.inputFlags = inputFlags;

    SendData(udpSocket, reinterpret_cast<const char*>(&packet), sizeof(packet), serverAddress);
}

void NetworkManager::ReceiveGameState() {
    char buffer[512];
    sockaddr_in clientAddr;
    int bytesReceived = ReceiveData(udpSocket, buffer, sizeof(buffer), clientAddr);
    if (bytesReceived > 0) {
        PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
        if (header->packetType == PT_GAME_STATE) {
            GameStatePacket* gameState = reinterpret_cast<GameStatePacket*>(buffer);
            // Update game state based on received data
        }
    }
}

void NetworkManager::HandleAcknowledgments() {
    char buffer[512];
    sockaddr_in clientAddr;
    int bytesReceived = ReceiveData(udpSocket, buffer, sizeof(buffer), clientAddr);
    if (bytesReceived > 0) {
        PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
        if (header->packetType == PT_ACK) {
            AckPacket* ack = reinterpret_cast<AckPacket*>(buffer);
            // Handle acknowledgment
        }
    }
}

void NetworkManager::HandleNewConnection(const sockaddr_in& clientAddr) {
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

    SendData(udpSocket, reinterpret_cast<const char*>(&packet), sizeof(packet), clientAddr);
}

void NetworkManager::HandlePlayerJoin(const PlayerJoinPacket& packet) {
    playerID = packet.playerID;
    std::cout << "Assigned Player ID: " << static_cast<int>(playerID) << std::endl;
}

void NetworkManager::UpdateGameState() {
    // Update player positions and handle game logic
    for (Player& player : players) {
        if (&player != nullptr && player.IsConnected() && player.GetShip() != nullptr) {
            player.GetShip()->posCurr.x += player.GetShip()->velCurr.x * g_dt;
            player.GetShip()->posCurr.y += player.GetShip()->velCurr.y * g_dt;
            // Handle collisions and other game logic here
        }
    }

    // Send game state updates to clients
    GameStatePacket gameStatePacket;
    gameStatePacket.header.packetType = PT_GAME_STATE;
    gameStatePacket.header.sequenceNumber = 0; // Set appropriate sequence number
    gameStatePacket.header.timestamp = static_cast<float>(g_appTime);
    gameStatePacket.playerCount = static_cast<unsigned char>(players.size());

    for (size_t i = 0; i < players.size(); ++i) {
        gameStatePacket.players[i].playerID = players[i].GetID();
        std::copy(players[i].GetName().begin(), players[i].GetName().end(), gameStatePacket.players[i].name);
        gameStatePacket.players[i].name[players[i].GetName().size()] = '\0'; // Null-terminate the string
        gameStatePacket.players[i].score = players[i].GetScore();
        gameStatePacket.players[i].isAlive = players[i].IsConnected();
    }

    gameStatePacket.objectCount = 0; // Update with actual object count if needed

    for (const Player& player : players) {
        if (player.IsConnected()) {
            SendData(udpSocket, reinterpret_cast<const char*>(&gameStatePacket), sizeof(gameStatePacket), serverAddress);
        }
    }
}

void NetworkManager::ProcessPlayerInput(unsigned char playerID, unsigned char inputFlags) {
    for (Player& player : players) {
        if (player.GetID() == playerID) {
            // Use the existing game state logic to update the ship's state
            if (inputFlags & INPUT_UP) {
                AEVec2 added;
                AEVec2Set(&added, cosf(player.GetShip()->dirCurr) * SHIP_ACCEL_FORWARD, sinf(player.GetShip()->dirCurr) * SHIP_ACCEL_FORWARD);
                AEVec2Scale(&added, &added, g_dt);
                AEVec2Add(&player.GetShip()->velCurr, &player.GetShip()->velCurr, &added);
                AEVec2Scale(&player.GetShip()->velCurr, &player.GetShip()->velCurr, SHIP_VELOCITY_CAP);
            }
            if (inputFlags & INPUT_DOWN) {
                AEVec2 added;
                AEVec2Set(&added, -cosf(player.GetShip()->dirCurr) * SHIP_ACCEL_BACKWARD, -sinf(player.GetShip()->dirCurr) * SHIP_ACCEL_BACKWARD);
                AEVec2Scale(&added, &added, g_dt);
                AEVec2Add(&player.GetShip()->velCurr, &player.GetShip()->velCurr, &added);
                AEVec2Scale(&player.GetShip()->velCurr, &player.GetShip()->velCurr, SHIP_VELOCITY_CAP);
            }
            if (inputFlags & INPUT_LEFT) {
                player.GetShip()->dirCurr += SHIP_ROT_SPEED * g_dt;
                player.GetShip()->dirCurr = AEWrap(player.GetShip()->dirCurr, -PI, PI);
            }
            if (inputFlags & INPUT_RIGHT) {
                player.GetShip()->dirCurr -= SHIP_ROT_SPEED * g_dt;
                player.GetShip()->dirCurr = AEWrap(player.GetShip()->dirCurr, -PI, PI);
            }
            if (inputFlags & INPUT_FIRE) {
                // Shoot a bullet
                AEVec2 bulletDirection;
                AEVec2Set(&bulletDirection, cosf(player.GetShip()->dirCurr), sinf(player.GetShip()->dirCurr));
                AEVec2 bulletVelocity;
                AEVec2Scale(&bulletVelocity, &bulletDirection, BULLET_SPEED);
                AEVec2 bulletPosition = player.GetShip()->posCurr;
                AEVec2 bulletScale;
                AEVec2Set(&bulletScale, BULLET_SCALE_X, BULLET_SCALE_Y);
                gameObjInstCreate(TYPE_BULLET, &bulletScale, &bulletPosition, &bulletVelocity, player.GetShip()->dirCurr);
            }
            player.SetLastUpdateTime(static_cast<float>(g_appTime));
            break;
        }
    }
}

void NetworkManager::Run() {
    char buffer[512];
    sockaddr_in clientAddr;
    while (true) {
        int bytesReceived = ReceiveData(udpSocket, buffer, sizeof(buffer), clientAddr);
        if (bytesReceived > 0) {
            // Handle received data
            PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
            if (isServer) {
                switch (header->packetType) {
                case PT_PLAYER_INPUT:
                    ProcessPlayerInput(reinterpret_cast<PlayerInputPacket*>(buffer)->playerID, reinterpret_cast<PlayerInputPacket*>(buffer)->inputFlags);
                    break;
                case PT_GAME_STATE:
                    // Handle game state update
                    break;
                case PT_ACK:
                    // Handle acknowledgment
                    break;
                case PT_PLAYER_JOIN:
                    HandleNewConnection(clientAddr);
                    break;
                default:
                    std::cerr << "Unknown packet type received" << std::endl;
                    break;
                }
            }
            else {
                switch (header->packetType) {
                case PT_PLAYER_INPUT:
                    // Handle player input
                    break;
                case PT_GAME_STATE:
                    // Handle game state update
                    break;
                case PT_ACK:
                    // Handle acknowledgment
                    break;
                case PT_PLAYER_JOIN:
                    HandlePlayerJoin(*reinterpret_cast<PlayerJoinPacket*>(buffer));
                    break;
                default:
                    std::cerr << "Unknown packet type received" << std::endl;
                    break;
                }
            }
        }

        if (isServer) {
            UpdateGameState();
        }
    }
}
void NetworkManager::AddPlayer(unsigned char id, const char* name) {
    
}
bool NetworkManager::sendPacketAndWaitForAck(SOCKET udpSocket, const std::vector<char>& packet, sockaddr_in& clientAddr, uint32_t sessionId, uint32_t offset) {
    while (true) {
        int sendResult = sendto(udpSocket, packet.data(), static_cast<int>(packet.size()), 0, reinterpret_cast<sockaddr*>(&clientAddr), sizeof(clientAddr));
        if (sendResult == SOCKET_ERROR) {
            std::cerr << "sendto() error: " << WSAGetLastError() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(udpSocket, &readSet);

        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
        int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);
        if (selectResult > 0 && FD_ISSET(udpSocket, &readSet)) {
            char ackBuffer[9];
            sockaddr_in fromAddr{};
            int fromAddrLen = sizeof(fromAddr);
            int recvLen = recvfrom(udpSocket, ackBuffer, 9, 0, reinterpret_cast<sockaddr*>(&fromAddr), &fromAddrLen);
            if (recvLen == 9) {
                unsigned char ackFlag = ackBuffer[0];
                if (ackFlag == 0x01) {
                    uint32_t ackSessionId, ackOffset;
                    memcpy(&ackSessionId, ackBuffer + 1, 4);
                    memcpy(&ackOffset, ackBuffer + 5, 4);

                    ackSessionId = ntohl(ackSessionId);
                    ackOffset = ntohl(ackOffset);

                    if (ackSessionId == sessionId && ackOffset == offset) {
                        return true;  // Correct acknowledgment, proceed to next chunk
                    }
                }
            }
        }
        else {
            std::cerr << "Timing out" << std::endl;
        }
    }
}