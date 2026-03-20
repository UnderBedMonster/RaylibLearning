#pragma once
#include <iostream>
#include <raylib.h>
#include "stb_perlin.h"
#include <cfloat>


class NoiseMap {

public:

	Mesh mesh = { 0 };
	Model model;

	float terrainWidth;
	float terrainDepth;
	float terrainAmplitude;
	float terrainScale;

	float minHeight = FLT_MAX;
	float maxHeight	= FLT_MIN;

	float lightIntensity = 5.0f;

	int lightPosLoc;
	int matModelLoc;
	int lightRadiusLoc;
	int lightIntensityLoc;
	Shader shader;
	
	NoiseMap(int w, int d, float scale, float amplitude) {
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

		FillVertices();
		FillIndices();

		UploadMesh(&mesh,0);

		model = LoadModelFromMesh(mesh);
	}
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
				if(y > maxHeight){
					maxHeight = y;
				}
				

				mesh.vertices[i * 3 + 0] = (float)x;
				mesh.vertices[i * 3 + 1] = y;
				mesh.vertices[i * 3 + 2] = (float)z;

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

	~NoiseMap() {}

	void SetShader(Shader& s) {
		model.materials[0].shader = s;
		shader = s;
		lightPosLoc = GetShaderLocation(shader, "lightPos");
		matModelLoc = GetShaderLocation(shader, "matModel");
		lightRadiusLoc = GetShaderLocation(shader, "lightRadius");
		lightIntensityLoc = GetShaderLocation(shader, "lightIntensity");
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

	void draw()
	{
		DrawModel(model, {1,1,1}, 1.0f, WHITE);
	};



private:


};



