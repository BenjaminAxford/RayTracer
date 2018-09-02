#pragma once

#include "Material.hpp"

class Metal : public Material
{
	Metal(Vec3f sc, float refl)
	{
		surfaceColour = sc;
		reflection = refl;
	}
};