#pragma once

#include "Vec3.hpp"

class Material
{
public:
	Vec3f surfaceColour = Vec3f(0,0,0);
	Vec3f emissionColour = Vec3f(0,0,0);
	float reflection = 1.0;
	float transparency = 0.0;

};