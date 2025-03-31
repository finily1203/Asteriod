/******************************************************************************/
/*!
\file		Main.h
\author 	
\par    	
\date   	
\brief		Header file for main.cpp. Also declares the function to spawn asteroids at random

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/


#ifndef CSD1130_MAIN_H_
#define CSD1130_MAIN_H_

//------------------------------------
// Globals

extern float	g_dt;
extern double	g_appTime;


// ---------------------------------------------------------------------------
// includes

#include "AEEngine.h"
#include "Math.h"
#include "GameStateMgr.h"
#include "GameState_Asteroids.h"
#include "Collision.h"

void spawnRandomAsteroid(void);

#endif











