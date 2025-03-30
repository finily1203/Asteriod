#include "Player.h"

Player::Player() : m_id(0), m_name("Unknown"), m_score(0), m_ship(nullptr), m_isConnected(false), m_lastUpdateTime(0.0f) {
}

Player::Player(unsigned char id, const char* name) 
    : 
    m_id(id),
    m_name(name),
    m_score(0),
    m_ship(nullptr),
    m_isConnected(true),
    m_lastUpdateTime(0.0f) {

}

Player::~Player() {
    // Destructor implementation (if needed)
}