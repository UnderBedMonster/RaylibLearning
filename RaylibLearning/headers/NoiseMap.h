#pragma once
#include <iostream>
#include <raylib.h>
#include "stb_perlin.h"
#include <cfloat>
#include <vector>
#include <raymath.h>
#include "Terrain.h"

// A Terrain shaped by 3D Perlin noise instead of being flat. Mesh building,
// shader wiring, drawing, and collision queries are all inherited from
// Terrain unchanged — this class only supplies ComputeHeight().
class NoiseMap : public Terrain {

public:

	float terrainAmplitude;
	float terrainScale;

	NoiseMap(Vector3 p, int w, int d, float scale, float amplitude)
		:Terrain(p, w, d) {

		terrainScale = scale;
		terrainAmplitude = amplitude;
	}

	float ComputeHeight(float x, float z) override {
		float nx = x * terrainScale;
		float nz = z * terrainScale;
		return stb_perlin_noise3(nx, 0.0f, nz, 0, 0, 0) * terrainAmplitude;
	}

	~NoiseMap() {}
};
