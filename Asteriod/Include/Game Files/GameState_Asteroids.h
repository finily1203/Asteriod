

#include "Collision.h"

#ifndef CSD1130_GAME_STATE_PLAY_H_
#define CSD1130_GAME_STATE_PLAY_H_

/******************************************************************************/
/*!
	Struct/Class Definitions
*/
/******************************************************************************/

//Game object structure
struct GameObj
{
	unsigned long		type;		// object type
	AEGfxVertexList* pMesh;		// This will hold the triangles which will form the shape of the object
};

// ---------------------------------------------------------------------------

//Game object instance structure
struct GameObjInst
{
	GameObj* pObject;	// pointer to the 'original' shape
	unsigned long		flag;		// bit flag or-ed together
	AEVec2				scale;		// scaling value of the object instance
	AEVec2				posCurr;	// object current position

	AEVec2				posPrev;	// object previous position -> it's the position calculated in the previous loop

	AEVec2				velCurr;	// object current velocity
	float				dirCurr;	// object current direction
	AABB				boundingBox;// object bounding box that encapsulates the object
	AEMtx33				transform;	// object transformation matrix: Each frame, 
	// calculate the object instance's transformation matrix and save it here

};

GameObjInst* gameObjInstCreate(unsigned long type, AEVec2* scale,
	AEVec2* pPos, AEVec2* pVel, float dir);

/******************************************************************************/

// ---------------------------------------------------------------------------

void GameStateAsteroidsLoad(void);
void GameStateAsteroidsInit(void);
void GameStateAsteroidsUpdate(void);
void GameStateAsteroidsDraw(void);
void GameStateAsteroidsFree(void);
void GameStateAsteroidsUnload(void);
// Add to GameState_Asteroids.h
// Add to GameState_Asteroids.h
GameObjInst* CreateAsteroidFromPacket(const AEVec2& position, const AEVec2& velocity, const AEVec2& scale);

void DestroyAsteroidById(unsigned short asteroidId);

// ---------------------------------------------------------------------------

#endif // CSD1130_GAME_STATE_PLAY_H_


