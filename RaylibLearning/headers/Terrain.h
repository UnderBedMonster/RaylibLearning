#pragma once
#include <iostream>
#include <raylib.h>
#include <cfloat>
#include <vector>
#include <raymath.h>

// Base terrain: builds a flat heightfield mesh (ComputeHeight() returns 0
// everywhere). Subclasses (e.g. NoiseMap) override ComputeHeight() to shape
// the terrain differently while reusing all the mesh/shader/collision code
// below unchanged.
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

	float minHeight = FLT_MAX;
	float maxHeight = FLT_MIN;

	Model model;
	Mesh mesh = { 0 };
	std::vector<std::vector<Vector3>> MapVertices;

	Terrain(Vector3 p, int w, int d) {

		Position = p;
		terrainWidth = w;
		terrainDepth = d;
	}

	// Height (before the Position.y offset) at local grid coordinates (x, z).
	// Override this to reshape the terrain; the default is flat (0 everywhere).
	virtual float ComputeHeight(float x, float z) {
		return 0.0f;
	}

	// Builds mesh/model from ComputeHeight(). Must be called once, explicitly,
	// after the object is fully constructed — NOT from a constructor. A
	// virtual call made during Terrain's own constructor would still resolve
	// to Terrain::ComputeHeight even for a NoiseMap, because the vtable isn't
	// switched over to the derived class until the derived constructor body
	// runs. Two-phase init sidesteps that: construct, then Generate().
	void Generate() {
		mesh = { 0 };
		mesh.vertexCount = (int)(terrainWidth * terrainDepth);
		mesh.triangleCount = (int)((terrainWidth - 1) * (terrainDepth - 1) * 2);

		mesh.vertices = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
		mesh.texcoords = (float*)MemAlloc(mesh.vertexCount * 2 * sizeof(float));
		mesh.indices = (unsigned short*)MemAlloc(mesh.triangleCount * 3 * sizeof(unsigned short));

		MapVertices.resize(terrainDepth, std::vector<Vector3>(terrainWidth));

		FillVertices();
		FillIndices();

		UploadMesh(&mesh, 0);
		model = LoadModelFromMesh(mesh);
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
		SetShaderValue(shader, GetShaderLocation(shader, "minHeight"), &minHeight, SHADER_UNIFORM_FLOAT);
		SetShaderValue(shader, GetShaderLocation(shader, "maxHeight"), &maxHeight, SHADER_UNIFORM_FLOAT);
		SetShaderValue(shader, lightIntensityLoc, &lightIntensity, SHADER_UNIFORM_FLOAT);
	}

	Shader getShader() {
		return shader;
	}

	void SetMaterialMapDiffuse(Texture2D& texture) {
		model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
	}

	void updateShader(Vector3 lightPos) {
		SetShaderValue(shader, lightPosLoc, &lightPos, SHADER_UNIFORM_VEC3);
		SetShaderValueMatrix(shader, matModelLoc, model.transform);
	}

	float getHeightAt(float worldX, float worldZ) {
		float localX = worldX - Position.x;
		float localZ = worldZ - Position.z;
		return ComputeHeight(localX, localZ) + Position.y;
	}

	Vector3 getClosestVertex(Vector3 spherePos) {
		// convert to grid coordinates
		int x = (int)round(spherePos.x - Position.x);
		int z = (int)round(spherePos.z - Position.z);

		// clamp to terrain bounds
		x = fmax(0, fmin(x, terrainWidth - 1));
		z = fmax(0, fmin(z, terrainDepth - 1));

		int i = z * terrainWidth + x;
		return {
			mesh.vertices[i * 3 + 0] + Position.x,
			mesh.vertices[i * 3 + 1] + Position.y,
			mesh.vertices[i * 3 + 2] + Position.z
		};
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

protected:
	void FillVertices() {
		minHeight = FLT_MAX;
		maxHeight = FLT_MIN;

		for (int z = 0; z < terrainDepth; z++) {
			for (int x = 0; x < terrainWidth; x++) {
				int i = z * terrainWidth + x;

				float y = ComputeHeight((float)x, (float)z);

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

		// A perfectly flat terrain leaves minHeight == maxHeight, and
		// terrain.frag divides by (maxHeight - minHeight) for height-based
		// coloring — keep that division defined.
		if (maxHeight - minHeight < 0.001f) {
			maxHeight = minHeight + 0.001f;
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
};
