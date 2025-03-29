/******************************************************************************/
/*!
\file       Player.h
\author     [Your Name]
\par        [Your Email]
\date       [Current Date]
\brief      Player class definition for multiplayer asteroids game

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/******************************************************************************/

#pragma once
#ifndef PLAYER_H
#define PLAYER_H

#include "AEEngine.h"
#include "GameState_Asteroids.cpp"

class Player
{
public:
    Player();
    Player(unsigned char id, const char* name);
    ~Player();

    // Getters
    unsigned char GetID() const { return m_id; }
    const char* GetName() const { return m_name; }
    int GetScore() const { return m_score; }
    GameObjInst* GetShip() const { return m_ship; }
    bool IsConnected() const { return m_isConnected; }
    float GetLastUpdateTime() const { return m_lastUpdateTime; }

    // Setters
    void SetScore(int score) { m_score = score; }
    void SetShip(GameObjInst* ship) { m_ship = ship; }
    void SetConnected(bool connected) { m_isConnected = connected; }
    void SetLastUpdateTime(float time) { m_lastUpdateTime = time; }

    // Ship control
    void UpdateShipFromInput(unsigned char inputFlags, float frameTime);
    void RespawnShip();

private:
    unsigned char m_id;          // Unique player ID (1-4)
    char m_name[32];             // Player name
    int m_score;                 // Current score
    GameObjInst* m_ship;         // Pointer to player's ship instance
    bool m_isConnected;          // Connection status
    float m_lastUpdateTime;      // Last time we received update from this player
};

// Input flags for compact network transmission
enum InputFlags {
    INPUT_UP = 1 << 0,
    INPUT_DOWN = 1 << 1,
    INPUT_LEFT = 1 << 2,
    INPUT_RIGHT = 1 << 3,
    INPUT_FIRE = 1 << 4
};

#endif // PLAYER_H