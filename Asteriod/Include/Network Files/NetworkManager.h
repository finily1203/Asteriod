#pragma once

#include <cstdint>



class NetworkManager {
public:
    enum PacketType {
        PT_PlayerInput,
        PT_GameState,
        PT_AsteroidSpawn,
        PT_AsteroidDestroy,
        PT_PlayerScore,
        PT_GameOver
    };

    struct GamePacket {
        PacketType type;
        uint32_t sequenceNumber;

    };

    bool Initialize(bool isServer, const char* serverIP = nullptr);
    void Update();
    void SendPlayerInput(const PlayerInput& input);
};