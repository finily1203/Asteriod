/******************************************************************************/
/*!
\file		GameState_Asteroids.cpp
\author 	
\par    	
\date   	
\brief		Source file for asteroid game state. Main game mechanics and interactions
			are coded here.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <iostream>

#include "main.h"
#include "GameState_Asteroids.h"
#include "Player.h"
#include "NetworkManager.h"

/******************************************************************************/
/*!
	Defines
*/
/******************************************************************************/
const unsigned int	GAME_OBJ_NUM_MAX		= 32;			// The total number of different objects (Shapes)
const unsigned int	GAME_OBJ_INST_NUM_MAX	= 2048;			// The total number of different game object instances


const unsigned int	SHIP_INITIAL_NUM		= 3;			// initial number of ship lives
const float			SHIP_SCALE_X			= 16.0f;		// ship scale x
const float			SHIP_SCALE_Y			= 16.0f;		// ship scale y
const float			BULLET_SCALE_X			= 20.0f;		// bullet scale x
const float			BULLET_SCALE_Y			= 3.0f;			// bullet scale y
const float			ASTEROID_MIN_SCALE_X	= 10.0f;		// asteroid minimum scale x
const float			ASTEROID_MAX_SCALE_X	= 60.0f;		// asteroid maximum scale x
const float			ASTEROID_MIN_SCALE_Y	= 10.0f;		// asteroid minimum scale y
const float			ASTEROID_MAX_SCALE_Y	= 60.0f;		// asteroid maximum scale y

const float			SHIP_VELOCITY_CAP		= 0.99f;		// ship velocity cap
const float			SHIP_ACCEL_FORWARD		= 100.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_ACCEL_BACKWARD		= 100.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_ROT_SPEED			= (2.0f * PI);	// ship rotation speed (degree/second)

const float			BULLET_SPEED			= 400.0f;		// bullet speed (m/s)

const float         BOUNDING_RECT_SIZE      = 1.0f;         // this is the normalized bounding rectangle (width and height) sizes - AABB collision data

// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// object flag definition

const unsigned long FLAG_ACTIVE				= 0x00000001;


/*!
	Static Variables
*/
/******************************************************************************/

// list of original object
static GameObj				sGameObjList[GAME_OBJ_NUM_MAX];				// Each element in this array represents a unique game object (shape)
static unsigned long		sGameObjNum;								// The number of defined game objects

// list of object instances
static GameObjInst			sGameObjInstList[GAME_OBJ_INST_NUM_MAX];	// Each element in this array represents a unique game object instance (sprite)
static unsigned long		sGameObjInstNum;							// The number of used game object instances

// pointer to the ship object
static GameObjInst *		spShip;										// Pointer to the "Ship" game object instance									

// number of ship available (lives 0 = game over)
static long					sShipLives;									// The number of lives left

// the score = number of asteroid destroyed
static unsigned long		sScore;										// Current score

// ---------------------------------------------------------------------------

// functions to create/destroy a game object instance

void				gameObjInstDestroy(GameObjInst * pInst);

static bool onValueChange = true;
static bool shipMovState = true;
/******************************************************************************/
/*!
	"Load" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsLoad(void)
{
	// zero the game object array
	memset(sGameObjList, 0, sizeof(GameObj) * GAME_OBJ_NUM_MAX);
	// No game objects (shapes) at this point
	sGameObjNum = 0;

	// zero the game object instance array
	memset(sGameObjInstList, 0, sizeof(GameObjInst) * GAME_OBJ_INST_NUM_MAX);
	// No game object instances (sprites) at this point
	sGameObjInstNum = 0;

	// The ship object instance hasn't been created yet, so this "spShip" pointer is initialized to 0
	spShip = nullptr;

	// load/create the mesh data (game objects / Shapes)
	GameObj* pObj;

	// =====================
	// create the ship shape
	// =====================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_SHIP;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFFFF0000, 0.0f, 0.0f,
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 0.0f,
		0.5f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =======================
	// create the bullet shape
	// =======================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BULLET;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =========================
	// create the asteroid shape
	// =========================
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_ASTEROID;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");
}

/******************************************************************************/
/*!
	"Initialize" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsInit(void)
{
	extern NetworkManager networkManager;
	// create the main ship
	AEVec2 scale;
	AEVec2Set(&scale, SHIP_SCALE_X, SHIP_SCALE_Y);
	spShip = gameObjInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
	AE_ASSERT(spShip);

	// Set the first player's ship
	if (!networkManager.GetPlayers().empty()) {
		networkManager.GetPlayers()[0].SetShip(spShip);
		std::cout << "Ship assigned to player 0" << std::endl;
	}
	else {
		std::cerr << "No players available to assign the ship" << std::endl;
	}

	// Initialize other players' ships
	for (auto& player : networkManager.GetPlayers()) {
		if (player.GetID() != 0) { // Skip the first player as it's already initialized
			AEVec2Set(&scale, SHIP_SCALE_X, SHIP_SCALE_Y);
			GameObjInst* newPlayerShip = gameObjInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
			AE_ASSERT(newPlayerShip);
			player.SetShip(newPlayerShip);
		}
	}

	// create the initial 4 asteroids instances using the "gameObjInstCreate" function
	AEVec2 pos, vel;

	// Asteroid 1
	pos.x = 90.0f; pos.y = -220.0f;
	vel.x = -60.0f; vel.y = -30.0f;
	AEVec2Set(&scale, ASTEROID_MIN_SCALE_X, ASTEROID_MAX_SCALE_Y);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	// Asteroid 2
	pos.x = -260.0f; pos.y = -250.0f;
	vel.x = 39.0f; vel.y = -130.0f;
	AEVec2Set(&scale, ASTEROID_MAX_SCALE_X, ASTEROID_MIN_SCALE_Y);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	// Asteroid 3
	pos.x = -27.0f; pos.y = -120.0f;
	vel.x = 45.0f; vel.y = -57.0f;
	AEVec2Set(&scale, ASTEROID_MAX_SCALE_X, ASTEROID_MIN_SCALE_Y);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	// Asteroid 4
	pos.x = -69.0f; pos.y = 240.0f;
	vel.x = -72.0f; vel.y = 123.0f;
	AEVec2Set(&scale, ASTEROID_MAX_SCALE_X, ASTEROID_MIN_SCALE_Y);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	// reset the score and the number of ships
	sScore = 0;
	sShipLives = SHIP_INITIAL_NUM;
}
/******************************************************************************/
/*!
	"Update" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsUpdate(void)
{
	// =========================================================
	// update according to input
	// =========================================================

	// This input handling moves the ship without any velocity nor acceleration
	// It should be changed when implementing the Asteroids project
	//
	// Updating the velocity and position according to acceleration is 
	// done by using the following:
	// Pos1 = 1/2 * a*t*t + v0*t + Pos0
	//
	// In our case we need to divide the previous equation into two parts in order 
	// to have control over the velocity and that is done by:
	//
	// v1 = a*t + v0		//This is done when the UP or DOWN key is pressed 
	// Pos1 = v1*t + Pos0
	f32 time = AEFrameRateControllerGetFrameTime();
	if (shipMovState)
	{
		if (AEInputCheckCurr(AEVK_UP))
		{
			// Find the velocity according to the acceleration
			// Limit your speed over here
			AEVec2 added;
			AEVec2Set(&added, cosf(spShip->dirCurr) * SHIP_ACCEL_FORWARD, sinf(spShip->dirCurr) * SHIP_ACCEL_FORWARD);

			// Apply added to velocity
			AEVec2Scale(&added, &added, static_cast<float>(time));
			AEVec2Add(&spShip->velCurr, &spShip->velCurr, &added);
			// Limit velocity to maximum speed (using SHIP_MAX_SPEED)
			AEVec2Scale(&spShip->velCurr, &spShip->velCurr, SHIP_VELOCITY_CAP);

		}

		if (AEInputCheckCurr(AEVK_DOWN))
		{
			// Find the velocity according to the deceleration
			// Limit your speed over here
			AEVec2 added;
			AEVec2Set(&added, -cosf(spShip->dirCurr) * SHIP_ACCEL_BACKWARD, -sinf(spShip->dirCurr) * SHIP_ACCEL_BACKWARD);

			// Apply acceleration to velocity
			AEVec2Scale(&added, &added, static_cast<float>(time));
			AEVec2Add(&spShip->velCurr, &spShip->velCurr, &added);

			// Limit velocity to maximum speed (200 units per second)
			AEVec2Scale(&spShip->velCurr, &spShip->velCurr, SHIP_VELOCITY_CAP);

		}

		if (AEInputCheckCurr(AEVK_LEFT))
		{
			spShip->dirCurr += SHIP_ROT_SPEED * (float)(AEFrameRateControllerGetFrameTime());
			spShip->dirCurr = AEWrap(spShip->dirCurr, -PI, PI);
		}

		if (AEInputCheckCurr(AEVK_RIGHT))
		{
			spShip->dirCurr -= SHIP_ROT_SPEED * (float)(AEFrameRateControllerGetFrameTime());
			spShip->dirCurr = AEWrap(spShip->dirCurr, -PI, PI);
		}

		// Separate position update outside the input checks// Calculate the displacement of the ship during the last frame
		AEVec2 displacement; AEVec2Scale(&displacement, &spShip->velCurr, (float)AEFrameRateControllerGetFrameTime());
		// Update the ship's position using the displacement
		AEVec2Add(&spShip->posCurr, &spShip->posCurr, &displacement);

		// Shoot a bullet if space is triggered (Create a new object instance)
		if (AEInputCheckTriggered(AEVK_SPACE))
		{
			// Get the bullet's direction according to the ship's direction
			// Set the velocity
			// Create an instance, based on BULLET_SCALE_X and BULLET_SCALE_Y

			// Step 1: Get the bullet's direction according to the ship's direction
			AEVec2 bulletDirection;
			AEVec2Set(&bulletDirection, cosf(spShip->dirCurr), sinf(spShip->dirCurr));

			// Step 2: Set the velocity of the bullet
			AEVec2 bulletVelocity;
			AEVec2Scale(&bulletVelocity, &bulletDirection, BULLET_SPEED);

			// Step 3: Create a new instance of the bullet object
			AEVec2 bulletPosition = spShip->posCurr; // Set the bullet's initial position to the ship's position
			AEVec2 bulletScale;
			AEVec2Set(&bulletScale, BULLET_SCALE_X, BULLET_SCALE_Y);

			// Call the function to create a new object instance for the bullet
			GameObjInst* newBullet = gameObjInstCreate(TYPE_BULLET, &bulletScale, &bulletPosition, &bulletVelocity, spShip->dirCurr);
		}

	}



	// ======================================================================
	// Save previous positions
	//  -- For all instances
	// [DO NOT UPDATE THIS PARAGRAPH'S CODE]
	// ======================================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		pInst->posPrev.x = pInst->posCurr.x;
		pInst->posPrev.y = pInst->posCurr.y;
	}






	// ======================================================================
	// update physics of all active game object instances
	//  -- Calculate the AABB bounding rectangle of the active instance, using the starting position:
	//		boundingRect_min = -(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->posPrev
	//		boundingRect_max = +(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->posPrev
	//
	//	-- New position of the active instance is updated here with the velocity calculated earlier
	// ======================================================================
	for (int i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		if (sGameObjInstList[i].flag && FLAG_ACTIVE)
		{
			GameObjInst* instance = sGameObjInstList + i;

			// Calculate the AABB bounding rectangle of the active instance
			AEVec2 boundingRect_min, boundingRect_max;
			boundingRect_min.x = -(BOUNDING_RECT_SIZE * instance->scale.x) / 2.0f + instance->posPrev.x;
			boundingRect_min.y = -(BOUNDING_RECT_SIZE * instance->scale.y) / 2.0f + instance->posPrev.y;
			boundingRect_max.x = (BOUNDING_RECT_SIZE * instance->scale.x) / 2.0f + instance->posPrev.x;
			boundingRect_max.y = (BOUNDING_RECT_SIZE * instance->scale.y) / 2.0f + instance->posPrev.y;

			// Update the new position of the active instance with the velocity calculated earlier
			instance->posCurr.x = instance->posPrev.x + (instance->velCurr.x * g_dt);
			instance->posCurr.y = instance->posPrev.y + (instance->velCurr.y * g_dt);

			// Update the bounding box of the instance
			instance->boundingBox.min = boundingRect_min;
			instance->boundingBox.max = boundingRect_max;
		}
	}

	// ======================================================================
	// check for dynamic-dynamic collisions
	// ======================================================================

	/*
	for each object instance: oi1
		if oi1 is not active
			skip

		if oi1 is an asteroid
			for each object instance oi2
				if(oi2 is not active or oi2 is an asteroid)
					skip

				if(oi2 is the ship)
					Check for collision between ship and asteroids (Rectangle - Rectangle)
					Update game behavior accordingly
					Update "Object instances array"
				else
				if(oi2 is a bullet)
					Check for collision between bullet and asteroids (Rectangle - Rectangle)
					Update game behavior accordingly
					Update "Object instances array"
	*/

	// ======================================================================
  // check for dynamic-dynamic collisions
  // ======================================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		float tFirst = 0.0f;
		GameObjInst* oi1 = sGameObjInstList + i;

		// Check if oi1 is active
		if ((oi1->flag && FLAG_ACTIVE) == 0)
			continue;

		// Check if oi1 is an asteroid
		if (oi1->pObject->type == TYPE_ASTEROID)
		{
			for (unsigned long j = 0; j < GAME_OBJ_INST_NUM_MAX; j++)
			{
				GameObjInst* oi2 = sGameObjInstList + j;

				// Check if oi2 is not active or oi2 is an asteroid
				if ((oi2->flag && FLAG_ACTIVE) == 0 || oi2->pObject->type == TYPE_ASTEROID)
					continue;

				// Check if oi2 is the ship
				if (oi2->pObject->type == TYPE_SHIP)
				{

					// Check for collision between ship and asteroids (Rectangle - Rectangle)
					if ((CollisionIntersection_RectRect(oi1->boundingBox, oi1->velCurr, oi2->boundingBox, oi2->velCurr, tFirst)) && (sScore < 5000))
					{
						// Handle collision between ship and asteroids
						// Update game behavior accordingly
						// Update "Object instances array"
						sShipLives--;
						gameObjInstDestroy(oi1);
						spShip->posCurr.x = 0.0f;
						spShip->posCurr.y = 0.0f;
						onValueChange = true;
						shipMovState = true;
						spawnRandomAsteroid();

						if (sShipLives < 0)
						{
							spShip->posCurr.x = 0.0f;
							spShip->posCurr.y = 0.0f;
							spShip->velCurr.x = 0.0f;
							spShip->velCurr.y = 0.0f;
							shipMovState = false;
						}
					}
				}
				else if (oi2->pObject->type == TYPE_BULLET)
				{

					// Check for collision between bullet and asteroids (Rectangle - Rectangle)
					if ((CollisionIntersection_RectRect(oi1->boundingBox, oi1->velCurr, oi2->boundingBox, oi2->velCurr, tFirst)) && shipMovState)
					{
						// Handle collision between bullet and asteroids
						// Update game behavior accordingly
						// Update "Object instances array"
						sScore += 100;
						onValueChange = true;
						shipMovState = true;
						gameObjInstDestroy(oi1);
						gameObjInstDestroy(oi2);
						oi1->flag &= !FLAG_ACTIVE;
						oi2->flag &= !FLAG_ACTIVE;
						spawnRandomAsteroid();
						spawnRandomAsteroid();

						if (sScore >= 5000)
						{
							shipMovState = false;
							sScore = 5000;
						}
					}
				}
			}
		}
	}





	// ===================================================================
	// update active game object instances
	// Example:
	//		-- Wrap specific object instances around the world (Needed for the assignment)
	//		-- Removing the bullets as they go out of bounds (Needed for the assignment)
	//		-- If you have a homing missile for example, compute its new orientation 
	//			(Homing missiles are not required for the Asteroids project)
	//		-- Update a particle effect (Not required for the Asteroids project)
	// ===================================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;
		
		// check if the object is a ship
		if (pInst->pObject->type == TYPE_SHIP)
		{
			// Wrap the ship from one end of the screen to the other
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - SHIP_SCALE_X, 
														AEGfxGetWinMaxX() + SHIP_SCALE_X);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - SHIP_SCALE_Y,
														AEGfxGetWinMaxY() + SHIP_SCALE_Y);
		}

		// Wrap asteroids here
		if (pInst->pObject->type == TYPE_ASTEROID)
		{
			// Wrap the ship from one end of the screen to the other
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - ASTEROID_MIN_SCALE_X,
														AEGfxGetWinMaxX() + ASTEROID_MAX_SCALE_X);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - ASTEROID_MIN_SCALE_Y,
														AEGfxGetWinMaxY() + ASTEROID_MAX_SCALE_Y);
		}
		// Remove bullets that go out of bounds
		
	}



	

	// =====================================================================
	// calculate the matrix for all objects
	// =====================================================================

	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;
		AEMtx33		 trans, rot, scale;

		UNREFERENCED_PARAMETER(trans);
		UNREFERENCED_PARAMETER(rot);
		UNREFERENCED_PARAMETER(scale);

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		// Compute the scaling matrix
		AEMtx33Scale(&scale, pInst -> scale.x, pInst -> scale.y);
		// Compute the rotation matrix 
		AEMtx33Rot(&rot, pInst -> dirCurr);
		// Compute the translation matrix
		AEMtx33Trans(&trans, pInst -> posCurr.x, pInst -> posCurr.y);
		// Concatenate the 3 matrix in the correct order in the object instance's "transform" matrix
		AEMtx33Concat(&pInst->transform, &trans, &rot);
		AEMtx33Concat(&pInst->transform, &pInst->transform, &scale);
	}
}

/******************************************************************************/
/*!
	
*/
/******************************************************************************/
void GameStateAsteroidsDraw(void)
{
	char strBuffer[1024];
	
	AEGfxSetRenderMode(AE_GFX_RM_COLOR);
	AEGfxTextureSet(NULL, 0, 0);


	// Set blend mode to AE_GFX_BM_BLEND
	// This will allow transparency.
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxSetTransparency(1.0f);


	// draw all object instances in the list
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		// Set the current object instance's transform matrix using "AEGfxSetTransform"
		AEGfxSetTransform((f32(*)[3]) & pInst->transform);
		// Draw the shape used by the current object instance using "AEGfxMeshDraw"
		AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
	}

	//You can replace this condition/variable by your own data.
	//The idea is to display any of these variables/strings whenever a change in their value happens
	if(onValueChange)
	{
		sprintf_s(strBuffer, "Score: %d", sScore);
		//AEGfxPrint(10, 10, (u32)-1, strBuffer);
		printf("%s \n", strBuffer);

		sprintf_s(strBuffer, "Ship Left: %d", sShipLives >= 0 ? sShipLives : 0);
		//AEGfxPrint(600, 10, (u32)-1, strBuffer);
		printf("%s \n", strBuffer);

		// display the game over message
		if (sShipLives < 0)
		{
			//AEGfxPrint(280, 260, 0xFFFFFFFF, "       GAME OVER       ");
			printf("       GAME OVER       \n");
		}

		// Display "You Rock" when game complete
		if (sScore == 5000)
		{
			printf("You Rock!\n");
		}
		onValueChange = false;
	}
}

/******************************************************************************/
/*!
	
*/
/******************************************************************************/
void GameStateAsteroidsFree(void)
{
	// kill all object instances in the array using "gameObjInstDestroy"
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;
		gameObjInstDestroy(pInst);
	}
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void GameStateAsteroidsUnload(void)
{
	// free all mesh data (shapes) of each object using "AEGfxTriFree"
	for (unsigned long i = 0; i < sGameObjNum; ++i)
	{
		AEGfxMeshFree(sGameObjList[i].pMesh);
		//if (sGameObjList[i].pMesh)
		//{
		//  AEGfxMeshFree(sGameObjList[i].pMesh);
		//  sGameObjList[i].pMesh = nullptr;
		//}
	}
}

/******************************************************************************/
/*!
	
*/
/******************************************************************************/
GameObjInst * gameObjInstCreate(unsigned long type, 
							   AEVec2 * scale,
							   AEVec2 * pPos, 
							   AEVec2 * pVel, 
							   float dir)
{
	AEVec2 zero;
	AEVec2Zero(&zero);

	AE_ASSERT_PARM(type < sGameObjNum);
	
	// loop through the object instance list to find a non-used object instance
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;

		// check if current instance is not used
		if (pInst->flag == 0)
		{
			// it is not used => use it to create the new instance
			pInst->pObject	= sGameObjList + type;
			pInst->flag		= FLAG_ACTIVE;
			pInst->scale	= *scale;
			pInst->posCurr	= pPos ? *pPos : zero;
			pInst->velCurr	= pVel ? *pVel : zero;
			pInst->dirCurr	= dir;
			
			// return the newly created instance
			return pInst;
		}
	}

	// cannot find empty slot => return 0
	return 0;
}

/******************************************************************************/
/*!
	
*/
/******************************************************************************/
void gameObjInstDestroy(GameObjInst * pInst)
{
	// if instance is destroyed before, just return
	if (pInst->flag == 0)
		return;

	// zero out the flag
	pInst->flag = 0;
}



/******************************************************************************/
/*!
    check for collision between Ship and Wall and apply physics response on the Ship
		-- Apply collision response only on the "Ship" as we consider the "Wall" object is always stationary
		-- We'll check collision only when the ship is moving towards the wall!
	[DO NOT UPDATE THIS PARAGRAPH'S CODE]
*/
/******************************************************************************/
void spawnRandomAsteroid()
{
	// Randomly determine which side of the screen the asteroid will spawn from
	int side = rand() % 4; // 0: top, 1: right, 2: bottom, 3: left

	AEVec2 randomPos;
	AEVec2 randomVel;
	  
	// Set initial position and velocity based on the side of the screen
	switch (side) {
	case 0: // Top
		randomPos.x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (AEGfxGetWinMaxX() - AEGfxGetWinMinX()) + AEGfxGetWinMinX();
		randomPos.y = AEGfxGetWinMaxY() + ASTEROID_MAX_SCALE_Y; // Spawn just above the top edge of the screen
		randomVel.x = 0; // No initial horizontal velocity
		randomVel.y = -static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 100; // Negative vertical velocity to move downwards
		break;
	case 1: // Right
		randomPos.x = AEGfxGetWinMaxX() + ASTEROID_MAX_SCALE_X; // Spawn just to the right of the right edge of the screen
		randomPos.y = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (AEGfxGetWinMaxY() - AEGfxGetWinMinY()) + AEGfxGetWinMinY();
		randomVel.x = -static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 100; // Negative horizontal velocity to move leftwards
		randomVel.y = 0; // No initial vertical velocity
		break;
	case 2: // Bottom
		randomPos.x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (AEGfxGetWinMaxX() - AEGfxGetWinMinX()) + AEGfxGetWinMinX();
		randomPos.y = AEGfxGetWinMinY() - ASTEROID_MAX_SCALE_Y; // Spawn just below the bottom edge of the screen
		randomVel.x = 0; // No initial horizontal velocity
		randomVel.y = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 100; // Positive vertical velocity to move upwards
		break;
	case 3: // Left
		randomPos.x = AEGfxGetWinMinX() - ASTEROID_MAX_SCALE_X; // Spawn just to the left of the left edge of the screen
		randomPos.y = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (AEGfxGetWinMaxY() - AEGfxGetWinMinY()) + AEGfxGetWinMinY();
		randomVel.x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 100; // Positive horizontal velocity to move rightwards
		randomVel.y = 0; // No initial vertical velocity
		break;
	}

	// Determine the direction of the asteroid based on its velocity
	float randomDir = atan2f(randomVel.y, randomVel.x);

	// Generate random scale for the asteroid
	AEVec2 randomScale;
	randomScale.x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X) + ASTEROID_MIN_SCALE_X;
	randomScale.y = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y) + ASTEROID_MIN_SCALE_Y;

	// Call the function to create the asteroid with the random parameters
	gameObjInstCreate(TYPE_ASTEROID, &randomScale, &randomPos, &randomVel, randomDir);
}
  