#pragma once
#include <iostream>
#include <raylib.h>
#include <cfloat>
#include <vector>
#include <raymath.h>   

class Terrain
{
public:


	Vector3 Position;
	float terrainWidth;
	float terrainDepth;

	Shader shader;
	int lightPosLoc;
	int matModelLoc;
	int lightRadiusLoc;
	int lightIntensityLoc;

	float lightIntensity = 5.0f;


	Model model;
	Mesh mesh = { 0 };
	std::vector<std::vector<Vector3>> MapVertices;



	Terrain(Vector3 p, int w, int d) {
	
		Position = p;
		terrainWidth = w;
		terrainDepth = d;
	}
	void virtual SetShader(Shader& s) {
		model.materials[0].shader = s;
		shader = s;
		lightPosLoc = GetShaderLocation(shader, "lightPos");
		matModelLoc = GetShaderLocation(shader, "matModel");
		lightRadiusLoc = GetShaderLocation(shader, "lightRadius");
		lightIntensityLoc = GetShaderLocation(shader, "lightIntensity");
		int terrainOffsetLoc = GetShaderLocation(shader, "terrainOffset");
		SetShaderValue(shader, terrainOffsetLoc, &Position, SHADER_UNIFORM_VEC3);
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
};

