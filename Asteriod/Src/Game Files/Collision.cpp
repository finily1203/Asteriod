

#include "..\Include\Game Files\main.h"

/**************************************************************************/
/*!

	*/
/**************************************************************************/
bool CollisionIntersection_RectRect(const AABB & aabb1,          //Input
									const AEVec2 & vel1,         //Input 
									const AABB & aabb2,          //Input 
									const AEVec2 & vel2,         //Input
									float& firstTimeOfCollision) //Output: the calculated value of tFirst, below, must be returned here
{
	UNREFERENCED_PARAMETER(aabb1);
	UNREFERENCED_PARAMETER(vel1);
	UNREFERENCED_PARAMETER(aabb2);
	UNREFERENCED_PARAMETER(vel2);
	UNREFERENCED_PARAMETER(firstTimeOfCollision);

	const AABB* rectA = &aabb1;
	const AABB* rectB = &aabb2;
	AEVec2 velA, velB, vRel;
	AEVec2Set(&velA, vel1.x, vel1.y);
	AEVec2Set(&velB, vel2.x, vel2.y);

	/*
	Implement the collision intersection over here.

	The steps are:	
	Step 1: Check for static collision detection between rectangles (static: before moving). 
				If the check returns no overlap, you continue with the dynamic collision test
					with the following next steps 2 to 5 (dynamic: with velocities).
				Otherwise you return collision is true, and you stop.

	Step 2: Initialize and calculate the new velocity of Vb
			tFirst = 0  //tFirst variable is commonly used for both the x-axis and y-axis
			tLast = dt  //tLast variable is commonly used for both the x-axis and y-axis

	Step 3: Working with one dimension (x-axis).
			if(Vb < 0)
				case 1
				case 4
			else if(Vb > 0)
				case 2
				case 3
			else //(Vb == 0)
				case 5

			case 6

	Step 4: Repeat step 3 on the y-axis

	Step 5: Return true: the rectangles intersect

	*/
	if (!(rectA -> max.x < rectB -> min.x|| rectA -> min.x > rectB -> max.x ||
		rectA -> max.y < rectB -> min.y || rectA -> min.y > rectB -> max.y)) 
	{
		return true; // Collision detected
	}


	f32 tFirst = 0;
	f32 tLast = AEFrameRateControllerGetFrameRate();
	
	AEVec2Sub(&vRel, &velB, &velA);

	if (vRel.x < 0)
	{
		if (rectA->min.x > rectB->max.x)
		{
			return false;
		}
		if (rectA->max.x < rectB->min.x)
		{
			tFirst = (rectA->max.x - rectB->min.x) / vRel.x;
			tLast = (rectA->min.x - rectB->max.x) / vRel.x;
		}
	}
	else if (vRel.x > 0)
	{
		if (rectA->max.x > rectB->min.x)
		{
			tLast = (rectA->max.x - rectB->min.x) / vRel.x;
		}
		if (rectA->max.x < rectB->min.x)
		{
			return false;
		}
	}
	else if (vRel.x == 0)
	{
		if (rectA->max.x < rectB->min.x)
		{
			return false;
		}
		if (rectA->min.x > rectB->max.x)
		{
			return false;
		}
	}

	if (vRel.y < 0)
	{
		if (rectA->min.y > rectB->max.y)
		{
			return false;
		}
		if (rectA->max.y < rectB->min.y)
		{
			tFirst = (rectA->max.y - rectB->min.y) / vRel.y;
			tLast = (rectA->min.y - rectB->max.y) / vRel.y;
		}
	}
	else if (vRel.y > 0)
	{
		if (rectA->max.y > rectB->min.y)
		{
			tLast = (rectA->max.y - rectB->min.y) / vRel.y;
		}
		if (rectA->max.y < rectB->min.y)
		{
			return false;
		}
	}
	else if (vRel.x == 0)
	{
		if (rectA->max.y < rectB->min.y)
		{
			return false;
		}
		if (rectA->min.y > rectB->max.y)
		{
			return false;
		}
	}

	if (tFirst > tLast)
	{
		return false;
	}

	return 0;
}