/******************************************************************************/
/*!
\file       InputState.h
\author     [Your Name]
\par        [Your Email]
\date       [Current Date]
\brief      Player input state definition for network transmission

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/******************************************************************************/

#ifndef INPUT_STATE_H
#define INPUT_STATE_H

#include "AEEngine.h"

/**
 * \brief Contains all input data needed to update a player's ship
 *
 * Matches the control scheme from GameState_Asteroids.cpp:
 * - UP/DOWN for acceleration
 * - LEFT/RIGHT for rotation
 * - SPACE for firing
 */
struct PlayerInput {
    uint8_t playerId;       // Which player this input is for (1-4)
    float timestamp;        // When the input was generated (for server reconciliation)

    // Movement inputs
    bool upPressed;         // W or Up Arrow - accelerate forward (SHIP_ACCEL_FORWARD)
    bool downPressed;       // S or Down Arrow - accelerate backward (SHIP_ACCEL_BACKWARD)
    bool leftPressed;       // A or Left Arrow - rotate left (SHIP_ROT_SPEED positive)
    bool rightPressed;      // D or Right Arrow - rotate right (SHIP_ROT_SPEED negative)

    // Action inputs
    bool firePressed;       // Spacebar - fire bullet (AEVK_SPACE)

    // Default constructor
    PlayerInput() :
        playerId(0), timestamp(0.0f),
        upPressed(false), downPressed(false),
        leftPressed(false), rightPressed(false),
        firePressed(false) {
    }

    // Helper method to convert to bit flags (for more compact network transmission)
    uint8_t ToFlags() const {
        return (upPressed ? 0x01 : 0) |
            (downPressed ? 0x02 : 0) |
            (leftPressed ? 0x04 : 0) |
            (rightPressed ? 0x08 : 0) |
            (firePressed ? 0x10 : 0);
    }

    // Helper method to set from bit flags
    void FromFlags(uint8_t flags) {
        upPressed = (flags & 0x01) != 0;
        downPressed = (flags & 0x02) != 0;
        leftPressed = (flags & 0x04) != 0;
        rightPressed = (flags & 0x08) != 0;
        firePressed = (flags & 0x10) != 0;
    }
};

#endif // INPUT_STATE_H