#pragma once
#include "Vec3.hpp"
#include "Material.hpp"

class SceneObject
{
public:
	Vec3f center;
	Vec3f surfaceColor, emissionColor;      /// surface color and emission (light) 
	float transparency, reflection;         /// surface transparency and reflectivity 
	Material material;
	virtual bool intersect(const Vec3f &rayorig, const Vec3f &raydir,
		float &t, float &u, float &v) const = 0;
};