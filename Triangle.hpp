#pragma once
#include "Vec3.hpp"
#include "Material.hpp"

constexpr float kEpsilon = 1e-8;

class Triangle : public SceneObject
{
public:
	Vec3f a, b, c;							/// position of the triangle vertices
	Triangle(
		const Vec3f &a,
		const Vec3f &b,
		const Vec3f &c,
		const Vec3f &sc,
		const float &refl = 0,
		const float &transp = 1.0,
		const Vec3f &ec = 0,
		const Material &mat = Material())
	{ /* empty */
		this->a = a;
		this->b = b;
		this->c = c;
		surfaceColor = sc;
		emissionColor = ec;
		transparency = transp;
		reflection = refl;
		material = mat;
	}
	//Compute a ray - sphere intersection using the geometric solution
	bool intersect(const Vec3f &rayorig, const Vec3f &raydir, float &t, float &u, float &v)  const
	{
		Vec3f ab = b - a;
		Vec3f ac = c - a;
		Vec3f pvec = raydir.crossProduct(ac);
		float det = ab.dot(pvec);

		if (det < kEpsilon) return false;

		if (fabs(det) < kEpsilon) return false;

		float invDet = 1 / det;

		Vec3f tvec = rayorig - a;
		u = tvec.dot(pvec) * invDet;
		if (u < 0 || u>1) return false;

		Vec3f qvec = tvec.crossProduct(ab);
		v = raydir.dot(qvec)*invDet;
		if (v < 0 || u + v >1) return false;

		t = ac.dot(qvec)*invDet;

		return true;
	}
};