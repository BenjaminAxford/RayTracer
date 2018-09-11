#pragma once
#include "Vec3.hpp"
#include "Material.hpp"

class Sphere : public SceneObject
{
public:
	/// position of the sphere 
	float radius, radius2;                  /// sphere radius and radius^2 
	Vec3f surfaceColor, emissionColor;      /// surface color and emission (light) 
	float transparency, reflection;         /// surface transparency and reflectivity 
	Material material;
	Sphere(
		const Vec3f &c,
		const float &r,
		const Vec3f &sc,
		const float &refl = 0,
		const float &transp = 1.0,
		const Vec3f &ec = 0,
		const Material &mat = Material())
	{
		center = c;
		radius = r;
		surfaceColor = sc;
		emissionColor = ec;
		transparency = transp;
		reflection = refl;
		radius2 = r * r;
		material = mat;
	}

	//Compute a ray - sphere intersection using the geometric solution
	bool intersect(const Vec3f &rayorig, const Vec3f &raydir, float &t0, float &t1, float &v) const
	{
		Vec3f l = center - rayorig;
		float tca = l.dot(raydir);
		if (tca < 0) return false;
		float d2 = l.dot(l) - tca * tca;
		if (d2 > radius2) return false;
		float thc = sqrt(radius2 - d2);
		t0 = tca - thc;
		t1 = tca + thc;

		return true;
	}
};