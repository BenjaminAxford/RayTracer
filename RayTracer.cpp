// RayTracer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <cstdlib> 
#include <cstdio> 
#include <cmath> 
#include <fstream> 
#include <vector> 
#include <iostream> 
#include <cassert> 
#include <algorithm>
#include <chrono>
#include <math.h>
#include <thread>

#include <SDL.h>
#include "ctpl_stl.h"

#include "Vec3.hpp"
#include "material.hpp"
#include "SceneObject.hpp"
#include "Sphere.hpp"
#include "Triangle.hpp"

#define MAX_RAY_DEPTH 5

#if defined __linux__ || defined __APPLE__ 
// "Compiled for Linux
#else 
// Windows doesn't define these values by default, Linux does
#define M_PI 3.141592653589793 
#define INFINITY 1e8 
#endif 


// Spiral generation of tiles
class SpiralOut {
protected:
	unsigned layer;
	unsigned leg;
public:
	int x, y; //read these as output from next, do not modify.
	SpiralOut() :layer(1), leg(0), x(0), y(0) {}
	void goNext() {
		switch (leg) {
		case 0: ++x; if (x == layer)  ++leg;                break;
		case 1: ++y; if (y == layer)  ++leg;                break;
		case 2: --x; if (-x == layer)  ++leg;                break;
		case 3: --y; if (-y == layer) { leg = 0; ++layer; }   break;
		}
	}
};






float mix(const float &a, const float &b, const float &mix)
{
	return b * mix + a * (1 - mix);
}

Vec3f trace(
	const Vec3f &rayorig,
	const Vec3f &raydir,
	const std::vector<SceneObject*> &scene,
	const int &depth)
{
	//if (raydir.length() != 1) std::cerr << "Error " << raydir << std::endl;
	float tnear = INFINITY;
	const SceneObject* sceneobject = NULL;
	// find intersection of this ray with the sphere in the scene
	for (unsigned i = 0; i < scene.size(); ++i) {
		float t0 = INFINITY, t1 = INFINITY, t2 = INFINITY;
		if (scene[i]->intersect(rayorig, raydir, t0, t1, t2)) {
			if (t0 < 0) t0 = t1;
			if (t0 < tnear) {
				tnear = t0;
				sceneobject = scene[i];
			}
		}
	}
	// if there's no intersection return black or background color
	if (!sceneobject) return Vec3f(2);

	/*std::cout << "Surface Colour: " << sceneobject->surfaceColor << std::endl;*/

	// Just return surface colour for now
	//return(sceneobject->surfaceColor);

	Vec3f surfaceColor = 0; // color of the ray/surfaceof the object intersected by the ray 
	Vec3f phit = rayorig + raydir * tnear; // point of intersection 
	Vec3f nhit = phit - sceneobject->center; // normal at the intersection point 
	nhit.normalize(); // normalize normal direction 
					  // If the normal and the view direction are not opposite to each other
					  // reverse the normal direction. That also means we are inside the sphere so set
					  // the inside bool to true. Finally reverse the sign of IdotN which we want
					  // positive.
	float bias = 1e-4; // add some bias to the point from which we will be tracing 
	bool inside = false;
	if (raydir.dot(nhit) > 0) nhit = -nhit, inside = true;
	if ((sceneobject->transparency > 0 || sceneobject->reflection > 0) && depth < MAX_RAY_DEPTH) {
		float facingratio = -raydir.dot(nhit);
		// change the mix value to tweak the effect
		float fresneleffect = mix(pow(1 - facingratio, 3), 1, 0.1);
		// compute reflection direction (not need to normalize because all vectors
		// are already normalized)
		Vec3f refldir = raydir - nhit * 2 * raydir.dot(nhit);
		refldir.normalize();
		Vec3f reflection = trace(phit + nhit * bias, refldir, scene, depth + 1);
		Vec3f refraction = 0;
		// if the sphere is also transparent compute refraction ray (transmission)
		if (sceneobject->transparency > 0) {
			float ior = 1.1, eta = (inside) ? ior : 1 / ior; // are we inside or outside the surface? 
			float cosi = -nhit.dot(raydir);
			float k = 1 - eta * eta * (1 - cosi * cosi);
			Vec3f refrdir = raydir * eta + nhit * (eta *  cosi - sqrt(k));
			refrdir.normalize();
			refraction = trace(phit - nhit * bias, refrdir, scene, depth + 1);
		}
		// the result is a mix of reflection and refraction (if the sphere is transparent)
		surfaceColor = (
			reflection * fresneleffect * sceneobject->reflection +
			refraction * (1 - fresneleffect) * sceneobject->transparency) * sceneobject->surfaceColor;
	}
	else {
		// it's a diffuse object, no need to raytrace any further
		for (unsigned i = 0; i < scene.size(); ++i) {
			if (scene[i]->emissionColor.x > 0) {
				// this is a light
				Vec3f transmission = 0;
				Vec3f lightDirection = scene[i]->center - phit;
				lightDirection.normalize();
				for (unsigned j = 0; j < scene.size(); ++j) {
					if (i != j) {
						float t0, t1, t2;
						if (scene[j]->intersect(phit + nhit * bias, lightDirection, t0, t1, t2)) {
							transmission = 1;
							break;
						}
					}
				}



				surfaceColor += sceneobject->surfaceColor * transmission *
					std::max(float(0), nhit.dot(lightDirection)) * scene[i]->emissionColor;

			}
		}
		//Vec3f reflection(0);

		//if (depth < MAX_RAY_DEPTH) {
		//	Vec3f refldir = raydir - nhit * 2 * raydir.dot(nhit);
		//	refldir.normalize();
		//	reflection = trace(phit + nhit * bias, refldir, spheres, depth + 1);
		//}
		////surfaceColor += reflection * 0.1;
	}

	return surfaceColor + sceneobject->emissionColor;
}

void threadedTrace(
	int id,
	const Vec3f &rayorig,
	const std::vector<SceneObject*> &scene,
	const int &depth,
	char* pixels,
	std::atomic<int>* totalrays,
	unsigned totalframes,
	unsigned width,
	unsigned height,
	unsigned raysperbatch,
	unsigned tiles,
	unsigned tilesj,
	unsigned tilesi)
{

	float invWidth = 1 / float(width), invHeight = 1 / float(height);
	float fov = 70, aspectratio = width / float(height);
	float angle = tan(M_PI * 0.5 * fov / 180.);

	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

	srand(millis);


	float tilewidth = (float)width / (float)tiles;
	float tileheight = (float)height / (float)tiles;


	//unsigned test = tilex + tiley;

	unsigned pixelsprocessed = 0;

	for (unsigned tiley = tilesi * tileheight; tiley < (tilesi * tileheight) + tileheight; tiley++)

		//for (unsigned tilex = 0; tilex <  128; tilex++)
	{
		for (unsigned tilex = tilesj * tilewidth; tilex < (tilesj * tilewidth) + tilewidth; tilex++)

			//for (tiley = 0; tiley < 128; tiley++)
		{
			pixelsprocessed++;

			/*	tilex = rand() % width;
				tiley = rand() % height;*/

				//std::cout << tiley << std::endl;

				//float xx = (2 * ((tilex + 0.5) * invWidth) - 1) * angle * aspectratio;
				//float yy = (1 - 2 * ((tiley + 0.5) * invHeight)) * angle;

			float xx = (2 * ((tilex)* invWidth) - 1) * angle * aspectratio;
			float yy = (1 - 2 * ((tiley)* invHeight)) * angle;

			Vec3f raydir(xx, yy, -1);
			raydir.normalize();

			Vec3f rotation = Vec3f(0, -0.142612, 0);
			//rotation = Vec3f(0, 0, -1).normalize();
			raydir += rotation;





			//Vec3f traceresult = trace(Vec3f(50 + sin(float(totalframes) / 100) * 100, 273 + sin(float(totalframes)/100)*100, -10000), raydir, spheres, 0);
			Vec3f traceresult = trace(Vec3f(sin(float(totalframes) / 250) * 50, 52, 295.6 + cos(float(totalframes) / 250) * 50    /* - (totalframes * 1)*/), raydir, scene, 0);
			//Vec3f traceresult = trace(Vec3f(0, 52, 295.6), raydir, spheres, 0);

			//// Draw edge lines for each tile
			//if (tiley == tilesi * tileheight)
			//{
			//	traceresult.x = 1.0;
			//	traceresult.y = 0.0;
			//	traceresult.z = 0.0;
			//}
			//if (tiley == (tilesi * tileheight) + tileheight - 1)
			//{
			//	traceresult.x = 0.0;
			//	traceresult.y = 1.0;
			//	traceresult.z = 0.0;
			//}


			//auto a = pixels + (tilex * 3 + tiley * width * 3); // this probbaly breaks shit
			auto a = pixels + (tilex + tiley * width) * 3; // this probbaly breaks shit
			auto b = a + 1;
			auto c = b + 1;

			*a = (unsigned char)(std::min(float(1), traceresult.x) * 255);
			*b = (unsigned char)(std::min(float(1), traceresult.y) * 255);
			*c = (unsigned char)(std::min(float(1), traceresult.z) * 255);
		}
		//std::cout << tiley << std::endl;
	}

	*totalrays += pixelsprocessed;

}

void render(const std::vector<SceneObject*> &scene)
{
	// Setup threadpool
	ctpl::thread_pool p(2 /* two threads in the pool */);

	// Setup tracing properties
	unsigned width = 1024, height = 768;
	Vec3f *image = new Vec3f[width * height], *pixel = image;
	float invWidth = 1 / float(width), invHeight = 1 / float(height);
	float fov = 45, aspectratio = width / float(height);
	float angle = tan(M_PI * 0.5 * fov / 180.);



	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return;
	}

	SDL_Window *window = SDL_CreateWindow("Hello World!", 100, 100, width, height,
		SDL_WINDOW_SHOWN);
	if (window == NULL) {
		std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		return;
	}


	int channels = 3; // for a RGB image
	char* pixels = new char[width * height * channels];

	// Total rays atomic store
	std::atomic<int>* totalrays = new std::atomic<int>;
	totalrays->store(0);

	unsigned totalframes = 0;

	auto renderstart = std::chrono::high_resolution_clock::now();

	while (true)
	{
		unsigned targetfps = 60;
		float framenanos = 1000000000 / targetfps;

		unsigned rays = 0;
		unsigned raysperbatch = 5000;

		auto start = std::chrono::high_resolution_clock::now();
		auto finish = std::chrono::high_resolution_clock::now();

		while (std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() <= framenanos)
		{
			/*if (totalrays >= 100)
				break;*/
				//std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() << " : " << frame_nanos << std::endl;

			unsigned tiles = 5;

			int gridoffset = tiles % 2 == 0 ? 1 : 0;

			SpiralOut spiralgrid;
			if (p.n_pending() <= tiles * tiles)
			{
				for (unsigned i = 0; i < tiles*tiles; i++)
				{
					int gridx = spiralgrid.x + tiles / 2 - gridoffset;
					int gridy = spiralgrid.y + tiles / 2 - gridoffset;

					p.push(threadedTrace, Vec3f(0, sin(float(totalframes) / 250), 0), scene, 0, pixels, totalrays, totalframes, width, height, raysperbatch, tiles, gridx, gridy);

					spiralgrid.goNext();
				}
			}

			finish = std::chrono::high_resolution_clock::now();
		}
		totalframes++;

		if (totalframes % 15 == 0)
		{
			float rps = (float(totalrays->load()) / std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - renderstart).count()) * 1000000000;
			auto totaltime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - renderstart).count() / 1000000000;
			auto fps = totaltime <= 0 ? 0 : totalframes / totaltime;
			std::cout << "Finished Frame, Total Rays: " << totalrays->load() << ", RPS: " << rps << ", FPS: " << fps << ", Time: " << totaltime << std::endl;
			std::cout << "Waiting Threads: " << p.n_pending() << std::endl;
		}



		SDL_Surface *surface = SDL_CreateRGBSurfaceFrom((void*)pixels,
			width,
			height,
			channels * 8,          // bits per pixel = 24
			width * channels,      // pitch
			0x0000FF,              // red mask
			0x00FF00,              // green mask
			0xFF0000,              // blue mask
			0);

		SDL_Surface *surface_window = SDL_GetWindowSurface(window);

		/*
		* Copies the bmp surface to the window surface
		*/
		SDL_BlitSurface(surface,
			NULL,
			surface_window,
			NULL);

		/*
		* Now updating the window
		*/
		SDL_UpdateWindowSurface(window);

		/*
		* This function used to update a window with OpenGL rendering.
		* This is used with double-buffered OpenGL contexts, which are the default.
		*/
		/*    SDL_GL_SwapWindow(window); */

		//SDL_Delay(10000);

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			/* handle your event here */
		}
	}

	/*    SDL_DestroyTexture(tex);*/
	/*    SDL_DestroyRenderer(ren);*/
	SDL_DestroyWindow(window);
	SDL_Quit();

}


int main(int argc, char *args[])
{
	srand(13);
	std::vector<SceneObject*> scene;

	// Camera is at Vec3f(50, 273, -10000)

	scene.push_back(new Sphere(Vec3f(-1e5 - 100, 40.8, 81.6), 1e5, Vec3f(0.75, 0.25, 0.25), 0, 0.0, Vec3f(0))); // Left
	scene.push_back(new Sphere(Vec3f(1e5 + 100, 40.8, 81.6), 1e5, Vec3f(0.25, 0.25, 0.75), 0, 0.0, Vec3f(0))); // Right
	scene.push_back(new Sphere(Vec3f(0, 40.8, -1e5 - 81.6), 1e5, Vec3f(0.25, 0.25, 0.25), 1.0, 0.0, Vec3f(0.0))); // Back
	scene.push_back(new Sphere(Vec3f(0, 40.8, 1e5 + 81.6), 1e5, Vec3f(0.75, 0.75, 0.75), 0, 0.0, Vec3f(0.0))); // Front
	scene.push_back(new Sphere(Vec3f(0, 1e5 + 120.6, 81.6), 1e5, Vec3f(0.75, 0.25, 0.75), 0, 0.0, Vec3f(0.0, 0.0, 0.0))); // Top
	scene.push_back(new Sphere(Vec3f(0, -1e5 - 60.8, 81.6), 1e5, Vec3f(0.75, 0.75, 0.25), 0, 0.0, Vec3f(0, 0, 0))); // Bottom

	// Spheres in box
	scene.push_back(new Sphere(Vec3f(-50, 16.5, 77), 1, Vec3f(1.0, 1.0, 1.0), 1.0, 0.0, Vec3f(0.0), Material())); // Mirror
	scene.push_back(new Sphere(Vec3f(50, 16.5, 78), 4.5, Vec3f(1.0, 1.0, 1.0), 0, 1, Vec3f(0.0), Material())); // Glass

	scene.push_back(new Sphere(Vec3f(0, 80.6, 50), 1, Vec3f(1.0, 1.0, 1.0), 1.0, 0, Vec3f(1, 1, 1))); // Light

	// Triangle
	scene.push_back(new Triangle(Vec3f(90, 30, 10), Vec3f(10, 50, -30), Vec3f(10, -30, 70), Vec3f(0.2, 1.0, 0.2), 0, 0, Vec3f(1.0, 1.0, 1.0)));


	render(scene);

	return 0;
}

