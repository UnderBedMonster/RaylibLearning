#pragma once
#include <iostream>
#include <raylib.h>
#include "stb_perlin.h"
#include <cfloat>
#include <vector>
#include <raymath.h>
#include "Terrain.h"


class NoiseMap : public Terrain {

public:

	float terrainAmplitude;
	float terrainScale;

	float minHeight = FLT_MAX;
	float maxHeight = FLT_MIN;

	float lightIntensity = 5.0f;

	NoiseMap(Vector3 p, int w, int d, float scale, float amplitude)
		:Terrain(p, w, d) {

		terrainScale = scale;
		terrainAmplitude = amplitude;

		mesh.vertexCount = terrainWidth * terrainDepth;
		mesh.triangleCount = (terrainWidth - 1) * (terrainDepth - 1) * 2;

		mesh.vertices = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
		mesh.texcoords = (float*)MemAlloc(mesh.vertexCount * 2 * sizeof(float));
		mesh.indices = (unsigned short*)MemAlloc(mesh.triangleCount * 3 * sizeof(unsigned short));

		MapVertices.resize(terrainDepth, std::vector<Vector3>(terrainWidth));

		FillVertices();
		FillIndices();

		UploadMesh(&mesh, 0);

		model = LoadModelFromMesh(mesh);
	}
	/*NoiseMap(Vector3 p, int w, int d, float scale, float amplitude) {

		Position = p;
		terrainWidth = w;
		terrainDepth = d;
		terrainScale = scale;
		terrainAmplitude = amplitude;
		mesh = { 0 };
		mesh.vertexCount = terrainWidth * terrainDepth;
		mesh.triangleCount = (terrainWidth - 1) * (terrainDepth - 1) * 2;

		mesh.vertices = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
		mesh.texcoords = (float*)MemAlloc(mesh.vertexCount * 2 * sizeof(float));
		mesh.indices = (unsigned short*)MemAlloc(mesh.triangleCount * 3 * sizeof(unsigned short));

		MapVertices.resize(terrainDepth, std::vector<Vector3>(terrainWidth));

		FillVertices();
		FillIndices();

		UploadMesh(&mesh, 0);

		model = LoadModelFromMesh(mesh);
	}*/
	void FillVertices() {
		for (int z = 0; z < terrainDepth; z++) {
			for (int x = 0; x < terrainWidth; x++) {
				int i = z * terrainWidth + x;

				float nx = x * terrainScale;
				float nz = z * terrainScale;
				float y = stb_perlin_noise3(nx, 0.0f, nz, 0, 0, 0) * terrainAmplitude;

				if (y < minHeight) {
					minHeight = y;
				}
				if (y > maxHeight) {
					maxHeight = y;
				}


				mesh.vertices[i * 3 + 0] = (float)x + Position.x;
				mesh.vertices[i * 3 + 1] = y + Position.y;
				mesh.vertices[i * 3 + 2] = (float)z + Position.z;

				MapVertices[z][x] = Vector3{ (float)x + Position.x ,y + Position.y, (float)z + Position.z };

				mesh.texcoords[i * 2 + 0] = (float)x / terrainWidth;
				mesh.texcoords[i * 2 + 1] = (float)z / terrainDepth;
			}
		}
	}
	void FillIndices() {
		int t = 0;
		for (int z = 0; z < terrainDepth - 1; z++) {
			for (int x = 0; x < terrainWidth - 1; x++) {
				int tl = z * terrainWidth + x;
				int tr = z * terrainWidth + x + 1;
				int bl = (z + 1) * terrainWidth + x;
				int br = (z + 1) * terrainWidth + x + 1;

				mesh.indices[t++] = tl;
				mesh.indices[t++] = bl;
				mesh.indices[t++] = tr;

				mesh.indices[t++] = tr;
				mesh.indices[t++] = bl;
				mesh.indices[t++] = br;
			}
		}
	}
	float getHeightAt(float worldX, float worldZ) {
		// convert world position to local terrain space
		float localX = worldX - Position.x;
		float localZ = worldZ - Position.z;

		// sample noise at local coordinates
		float nx = localX * terrainScale;
		float nz = localZ * terrainScale;
		return stb_perlin_noise3(nx, 0.0f, nz, 0, 0, 0) * terrainAmplitude + Position.y;
		//                                                                    ↑
		//                                                       add Y offset for world height
	}

	~NoiseMap() {}

	

	void SetShader(Shader& s) override {
		model.materials[0].shader = s;
		shader = s;
		lightPosLoc = GetShaderLocation(shader, "lightPos");
		matModelLoc = GetShaderLocation(shader, "matModel");
		lightRadiusLoc = GetShaderLocation(shader, "lightRadius");
		lightIntensityLoc = GetShaderLocation(shader, "lightIntensity");
		int terrainOffsetLoc = GetShaderLocation(shader, "terrainOffset");
		SetShaderValue(shader, terrainOffsetLoc, &Position, SHADER_UNIFORM_VEC3);

		SetShaderValue(shader, GetShaderLocation(shader, "minHeight"), &minHeight, SHADER_UNIFORM_FLOAT);
		SetShaderValue(shader, GetShaderLocation(shader, "maxHeight"), &maxHeight, SHADER_UNIFORM_FLOAT);
		SetShaderValue(shader, lightIntensityLoc, &lightIntensity, SHADER_UNIFORM_FLOAT);
	}

	void SetMaterialMapDiffuse(Texture2D& texture) {
		model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
	}

	void updateShader(Vector3 lightPos) {
		SetShaderValue(shader, lightPosLoc, &lightPos, SHADER_UNIFORM_VEC3);
		SetShaderValueMatrix(shader, matModelLoc, model.transform);
	}

	void drawNormals(float length = 1.0f, int step = 1) {
		for (int z = 1; z < terrainDepth - 1; z += step) {
			for (int x = 1; x < terrainWidth - 1; x += step) {
				int i = z * terrainWidth + x;

				// get this vertex and its 4 neighbors
				Vector3 center = {
					mesh.vertices[i * 3 + 0],
					mesh.vertices[i * 3 + 1],
					mesh.vertices[i * 3 + 2]
				};

				int iL = z * terrainWidth + (x - 1);
				int iR = z * terrainWidth + (x + 1);
				int iU = (z - 1) * terrainWidth + x;
				int iD = (z + 1) * terrainWidth + x;

				Vector3 left = { mesh.vertices[iL * 3 + 0], mesh.vertices[iL * 3 + 1], mesh.vertices[iL * 3 + 2] };
				Vector3 right = { mesh.vertices[iR * 3 + 0], mesh.vertices[iR * 3 + 1], mesh.vertices[iR * 3 + 2] };
				Vector3 up = { mesh.vertices[iU * 3 + 0], mesh.vertices[iU * 3 + 1], mesh.vertices[iU * 3 + 2] };
				Vector3 down = { mesh.vertices[iD * 3 + 0], mesh.vertices[iD * 3 + 1], mesh.vertices[iD * 3 + 2] };

				// two edge vectors across this vertex
				Vector3 dx = Vector3Subtract(right, left);
				Vector3 dz = Vector3Subtract(down, up);

				// cross product gives the normal
				Vector3 normal = Vector3Normalize(Vector3CrossProduct(dz, dx));

				// offset center by Position for world space
				Vector3 worldCenter = Vector3Add(center, Position);
				Vector3 tip = Vector3Add(worldCenter, Vector3Scale(normal, length));

				DrawLine3D(worldCenter, tip, GREEN);
			}
		}
	}

	void draw()
	{
		DrawModel(model, Position, 1.0f, WHITE);
	};

private:
};