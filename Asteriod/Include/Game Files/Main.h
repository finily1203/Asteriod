


#ifndef CSD1130_MAIN_H_
#define CSD1130_MAIN_H_
#include <atomic>

//------------------------------------
// Globals

extern float	g_dt;
extern double	g_appTime;



extern std::atomic<bool> g_isRunning;
// ---------------------------------------------------------------------------
// includes

#include "AEEngine.h"
#include "Math.h"
#include "GameStateMgr.h"
#include "GameState_Asteroids.h"
#include "Collision.h"
void spawnRandomAsteroid(void);

#endif











