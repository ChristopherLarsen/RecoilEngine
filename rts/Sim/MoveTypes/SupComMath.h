/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_MATH_H
#define SUPCOM_MATH_H

#include "System/SpringMath.h"

// supcom: the heading of a direction for our move types. Stock GetHeadingFromVectorF keeps its divisor off zero with a
// pad of 1e-6 * (2 dz - 1), which for 0 < dz < 1e-6 turns the divisor negative: the heading comes out 180 degrees off
// (a direction exactly along x, after a rotation into the up-vector frame, has dz of a few 1e-8). Such a dz is taken as
// 0 here, which gives the axis heading; every other direction gets the stock heading. Stock code keeps the stock function.
inline short int SupComHeadingFromVector(float dx, float dz)
{
	if (dz > 0.0f && dz < 0.000001f)
		dz = 0.0f;

	return GetHeadingFromVector(dx, dz);
}

#endif
