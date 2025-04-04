#include <stdio.h>
#include <string.h>
#include <enet/enet.h>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <chrono>
#include <sstream>
#include <random>
#include <iostream>

// Windows-specific headers
#ifdef _WIN32
#include <winsock2.h>
#endif

#define PORT 7777
#define BUFFER_SIZE 2048

struct PlayerData {
    int playerID;
    float x, y, rot;
    ENetPeer* peer;
    std::chrono::steady_clock::time_point lastActive;
    int score = 0;
    int lives = 3;
};

struct BulletData {
    int bulletID;
    int ownerID;
    float x, y, velX, velY, rot;
    std::chrono::steady_clock::time_point creationTime;
};

struct AsteroidData {
    int asteroidID;
    float x, y, velX, velY, scaleX, scaleY;
};

std::unordered_map<int, PlayerData> players;
std::unordered_map<int, BulletData> activeBullets;
std::unordered_map<int, AsteroidData> activeAsteroids;
std::mutex playersMutex, bulletsMutex, asteroidsMutex;
int nextPlayerID = 1;
int nextBulletID = 1;
int nextAsteroidID = 1;

void spawnAsteroid(ENetHost* server);
void handleCollisions(ENetHost* server);
bool checkCollision(const BulletData& bullet, const AsteroidData& asteroid);
bool checkCollisionPlayer(const PlayerData& player, const AsteroidData& asteroid);
void sendPlayerDataBroadcast(ENetHost* server);

int main() {
    if (enet_initialize() != 0) {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = PORT;

    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (server == NULL) {
        fprintf(stderr, "An error occurred while trying to create an ENet server host.\n");
        return EXIT_FAILURE;
    }

    printf("Server started on port %d\n", PORT);

    auto lastUpdateTime = std::chrono::steady_clock::now();
    auto lastSpawnTime = std::chrono::steady_clock::now();

    while (true) {
        ENetEvent event;
        while (enet_host_service(server, &event, 1000) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                printf("A new client connected from %x:%u.\n",
                    event.peer->address.host,
                    event.peer->address.port);

                {
                    std::lock_guard<std::mutex> lock(playersMutex);
                    int newPlayerID = nextPlayerID++;
                    PlayerData newPlayer = { newPlayerID, 0.0f, 0.0f, 0.0f, event.peer, std::chrono::steady_clock::now() };
                    players[newPlayerID] = newPlayer;

                    // Send assigned ID back to the client
                    ENetPacket* packet = enet_packet_create(&newPlayerID, sizeof(newPlayerID), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, packet);
                }
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                printf("A packet of length %zu was received from %x:%u on channel %u.\n",
                    event.packet->dataLength,
                    event.peer->address.host,
                    event.peer->address.port,
                    event.channelID);

                {
                    std::string receivedPayload((char*)event.packet->data, event.packet->dataLength);
                    std::stringstream cmdStream(receivedPayload);
                    std::string command;
                    cmdStream >> command;

                    if (command == "PLAYER") {
                        int playerID;
                        float x, y, rot;
                        if (cmdStream >> playerID >> x >> y >> rot) {
                            std::lock_guard<std::mutex> lock(playersMutex);
                            auto it = players.find(playerID);
                            if (it != players.end()) {
                                PlayerData& player = it->second;
                                player.x = x;
                                player.y = y;
                                player.rot = rot;
                                player.lastActive = std::chrono::steady_clock::now();
                            }
                        }
                    }
                    else if (command == "SHOOT") {
                        int playerID;
                        if (cmdStream >> playerID) {
                            std::lock_guard<std::mutex> lock(playersMutex);
                            auto it = players.find(playerID);
                            if (it != players.end()) {
                                PlayerData& shooter = it->second;
                                std::lock_guard<std::mutex> bulletLock(bulletsMutex);
                                BulletData newBullet = { nextBulletID++, playerID, shooter.x, shooter.y, cosf(shooter.rot) * 400.0f, sinf(shooter.rot) * 400.0f, shooter.rot, std::chrono::steady_clock::now() };
                                activeBullets[newBullet.bulletID] = newBullet;
                            }
                        }
                    }
                }

                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                printf("%x:%u disconnected.\n",
                    event.peer->address.host,
                    event.peer->address.port);

                {
                    std::lock_guard<std::mutex> lock(playersMutex);
                    for (auto it = players.begin(); it != players.end(); ++it) {
                        if (it->second.peer == event.peer) {
                            players.erase(it);
                            break;
                        }
                    }
                }
                break;

            case ENET_EVENT_TYPE_NONE:
                break;
            }
        }

        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = currentTime - lastUpdateTime;
        float deltaTime = elapsed.count();
        lastUpdateTime = currentTime;

        {
            std::lock_guard<std::mutex> lock(bulletsMutex);
            for (auto it = activeBullets.begin(); it != activeBullets.end(); ) {
                BulletData& bullet = it->second;
                bullet.x += bullet.velX * deltaTime;
                bullet.y += bullet.velY * deltaTime;

                if (std::chrono::steady_clock::now() - bullet.creationTime > std::chrono::seconds(3)) {
                    it = activeBullets.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(asteroidsMutex);
            for (auto& [id, asteroid] : activeAsteroids) {
                asteroid.x += asteroid.velX * deltaTime;
                asteroid.y += asteroid.velY * deltaTime;

                if (asteroid.x < -400.0f) asteroid.x += 800.0f;
                if (asteroid.x > 400.0f) asteroid.x -= 800.0f;
                if (asteroid.y < -300.0f) asteroid.y += 600.0f;
                if (asteroid.y > 300.0f) asteroid.y -= 600.0f;
            }
        }

        handleCollisions(server);

        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastSpawnTime).count() >= 5) {
            spawnAsteroid(server);
            lastSpawnTime = currentTime;
        }

        sendPlayerDataBroadcast(server);
    }

    enet_host_destroy(server);
    return 0;
}

void spawnAsteroid(ENetHost* server) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> posDistX(-300.f, 300.f);
    static std::uniform_real_distribution<float> posDistY(-150.f, 150.f);
    static std::uniform_real_distribution<float> velDist(-30.0f, 15.0f);
    static std::uniform_real_distribution<float> scaleDist(60.0f, 100.0f);

    float x = posDistX(gen);
    float y = posDistY(gen);
    float velX = velDist(gen);
    float velY = velDist(gen);
    float scaleX = scaleDist(gen);
    float scaleY = scaleDist(gen);

    int asteroidID = nextAsteroidID++;
    AsteroidData newAsteroid = { asteroidID, x, y, velX, velY, scaleX, scaleY };

    {
        std::lock_guard<std::mutex> lock(asteroidsMutex);
        activeAsteroids[asteroidID] = newAsteroid;
    }

    std::string broadcastMessage = "ASTEROID_CREATE " + std::to_string(asteroidID) + " " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(velX) + " " + std::to_string(velY) + " " + std::to_string(scaleX) + " " + std::to_string(scaleY) + "\n";

    std::lock_guard<std::mutex> playerLock(playersMutex);
    for (const auto& [id, player] : players) {
        ENetPacket* packet = enet_packet_create(broadcastMessage.c_str(), broadcastMessage.length(), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(player.peer, 0, packet);
    }
}

void handleCollisions(ENetHost* server) {
    {
        std::lock_guard<std::mutex> bulletLock(bulletsMutex);
        std::lock_guard<std::mutex> asteroidLock(asteroidsMutex);
        for (auto bulletIt = activeBullets.begin(); bulletIt != activeBullets.end(); ) {
            const auto& bullet = bulletIt->second;
            bool collisionOccurred = false;
            for (auto asteroidIt = activeAsteroids.begin(); asteroidIt != activeAsteroids.end(); ) {
                const auto& asteroid = asteroidIt->second;
                if (checkCollision(bullet, asteroid)) {
                    int destroyedAsteroidID = asteroidIt->first;
                    int destroyedBulletID = bulletIt->first;
                    {
                        std::lock_guard<std::mutex> playerLock(playersMutex);
                        for (auto& [id, player] : players) {
                            if (player.playerID == bullet.ownerID) {
                                player.score += 100;
                                break;
                            }
                        }
                    }
                    bulletIt = activeBullets.erase(bulletIt);
                    asteroidIt = activeAsteroids.erase(asteroidIt);
                    std::string asteroidDestroyMessage = "ASTEROID_DESTROY " + std::to_string(destroyedAsteroidID) + "\n";
                    std::string bulletDestroyMessage = "BULLET_DESTROY " + std::to_string(destroyedBulletID) + "\n";
                    {
                        std::lock_guard<std::mutex> playerLock(playersMutex);
                        for (const auto& [id, player] : players) {
                            ENetPacket* packet = enet_packet_create(asteroidDestroyMessage.c_str(), asteroidDestroyMessage.length(), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(player.peer, 0, packet);
                            packet = enet_packet_create(bulletDestroyMessage.c_str(), bulletDestroyMessage.length(), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(player.peer, 0, packet);
                        }
                    }
                    collisionOccurred = true;
                    break;
                }
                else {
                    ++asteroidIt;
                }
            }
            if (!collisionOccurred) {
                ++bulletIt;
            }
        }
    }

    {
        std::lock_guard<std::mutex> playerLock(playersMutex);
        std::lock_guard<std::mutex> asteroidLock(asteroidsMutex);
        for (auto& [id, player] : players) {
            for (auto asteroidIt = activeAsteroids.begin(); asteroidIt != activeAsteroids.end(); ) {
                const auto& asteroid = asteroidIt->second;
                if (checkCollisionPlayer(player, asteroid)) {
                    player.lives -= 1;
                    std::string collisionMsg = "COLLISION " + std::to_string(player.playerID) + " " + std::to_string(asteroidIt->first) + "\n";
                    for (const auto& [id, pData] : players) {
                        ENetPacket* packet = enet_packet_create(collisionMsg.c_str(), collisionMsg.length(), ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(pData.peer, 0, packet);
                    }
                    asteroidIt = activeAsteroids.erase(asteroidIt);
                    break;
                }
                else {
                    ++asteroidIt;
                }
            }
        }
    }
}

bool checkCollision(const BulletData& bullet, const AsteroidData& asteroid) {
    float bulletHalfWidth = 20.0f / 2.0f;
    float bulletHalfHeight = 20.0f / 2.0f;
    float asteroidHalfWidth = asteroid.scaleX / 2.0f;
    float asteroidHalfHeight = asteroid.scaleY / 2.0f;

    bool overlapX = (bullet.x + bulletHalfWidth > asteroid.x - asteroidHalfWidth) &&
        (asteroid.x + asteroidHalfWidth > bullet.x - bulletHalfWidth);
    bool overlapY = (bullet.y + bulletHalfHeight > asteroid.y - asteroidHalfHeight) &&
        (asteroid.y + asteroidHalfHeight > bullet.y - bulletHalfHeight);

    return overlapX && overlapY;
}

bool checkCollisionPlayer(const PlayerData& player, const AsteroidData& asteroid) {
    const float shipWidth = 16.0f;
    const float shipHeight = 16.0f;

    float playerHalfWidth = shipWidth / 2.0f;
    float playerHalfHeight = shipHeight / 2.0f;
    float asteroidHalfWidth = asteroid.scaleX / 2.0f;
    float asteroidHalfHeight = asteroid.scaleY / 2.0f;

    bool overlapX = (player.x + playerHalfWidth > asteroid.x - asteroidHalfWidth) &&
        (asteroid.x + asteroidHalfWidth > player.x - playerHalfWidth);
    bool overlapY = (player.y + playerHalfHeight > asteroid.y - asteroidHalfHeight) &&
        (asteroid.y + asteroidHalfHeight > player.y - playerHalfHeight);

    return overlapX && overlapY;
}

void sendPlayerDataBroadcast(ENetHost* server) {
    std::string broadcastMessage;

    {
        std::lock_guard<std::mutex> playerLock(playersMutex);
        if (!players.empty()) {
            std::stringstream playerStream;
            for (const auto& [id, player] : players) {
                playerStream << "PLAYER " << player.playerID << " " << player.x << " " << player.y << " " << player.rot << " " << player.score << "\n";
            }
            broadcastMessage += playerStream.str();
        }
    }

    {
        std::lock_guard<std::mutex> bulletLock(bulletsMutex);
        if (!activeBullets.empty()) {
            std::stringstream bulletStream;
            for (const auto& [id, bullet] : activeBullets) {
                bulletStream << "BULLET " << bullet.bulletID << " " << bullet.ownerID << " " << bullet.x << " " << bullet.y << " " << bullet.rot << "\n";
            }
            broadcastMessage += bulletStream.str();
        }
    }

    {
        std::lock_guard<std::mutex> asteroidLock(asteroidsMutex);
        for (const auto& [id, asteroid] : activeAsteroids) {
            broadcastMessage += "ASTEROID_UPDATE " + std::to_string(id) + " " + std::to_string(asteroid.x) + " " + std::to_string(asteroid.y) + "\n";
        }
    }

    if (broadcastMessage.empty()) {
        return;
    }

    std::lock_guard<std::mutex> playerLock(playersMutex);
    for (const auto& [id, player] : players) {
        ENetPacket* packet = enet_packet_create(broadcastMessage.c_str(), broadcastMessage.length(), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(player.peer, 0, packet);
    }
}