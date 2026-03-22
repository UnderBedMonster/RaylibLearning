#pragma once
#include <iostream>
#include <raylib.h>
#include <cfloat>
#include <vector>
#include <raymath.h>   
#include <cstdlib>

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

	std::vector<float> deformation;  // deformation per vertex
	float maxDeform = 0.0f;        // track max for normalization
	int deformColorLoc;

	const float MAX_DEFORM = 140.;

	std::vector<float> velocity_d;  // deformation velocity per vertex (like a wave)
	float damping = 0.98f;       // energy loss per frame
	float stiffness = 0.3f;

	struct ImpactEvent {
		Vector3 center;
		float   currentRadius = 0.0f;
		float   maxRadius;
		float   strength;
		float   spreadSpeed = 8.0f;
		bool    active = true;
	};

	std::vector<ImpactEvent> impacts;

	Model model;
	Mesh mesh = { 0 };
	std::vector<std::vector<Vector3>> MapVertices;

	Terrain(Vector3 p, int w, int d) {
		Position = p;
		terrainWidth = w;
		terrainDepth = d;
		mesh = { 0 };
		mesh.vertexCount = terrainWidth * terrainDepth;
		mesh.triangleCount = (terrainWidth - 1) * (terrainDepth - 1) * 2;

		mesh.vertices = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
		mesh.texcoords = (float*)MemAlloc(mesh.vertexCount * 2 * sizeof(float));
		mesh.indices = (unsigned short*)MemAlloc(mesh.triangleCount * 3 * sizeof(unsigned short));

		FillVertices();
		FillIndices();

		// ✅ colors allocated ONCE before UploadMesh
		mesh.colors = (unsigned char*)MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));
		memset(mesh.colors, 0, mesh.vertexCount * 4 * sizeof(unsigned char));
		for (int i = 0; i < mesh.vertexCount; i++) mesh.colors[i * 4 + 3] = 255;

		UploadMesh(&mesh, true);        // ✅ true = dynamic
		model = LoadModelFromMesh(mesh);
		deformation.resize(mesh.vertexCount, 0.0f);
	}


	void addImpact(Vector3 pos, float strength) {
		float spreadSpeed = 50.f;           // faster
		float maxRadius = 40.0f;           // fixed max, not strength dependent
		impacts.push_back({ pos, 0.0f, maxRadius, strength, spreadSpeed, true });
	}

	void propagateDeformation(float deltaTime) {
		if (impacts.empty()) return;

		for (auto& impact : impacts) {
			if (!impact.active) continue;

			impact.currentRadius += impact.spreadSpeed * deltaTime;
			impact.strength *= (1.0f - 1.5f * deltaTime);  // was 1.5f → 5.0f — dies much faster

			if (impact.currentRadius >= impact.maxRadius || impact.strength < 0.01f) {
				impact.active = false;
				continue;
			}

			float ringWidth = 4.0f;
			int   affected = 0;

			for (int z = 0; z < terrainDepth; z++) {
				for (int x = 0; x < terrainWidth; x++) {
					int i = z * terrainWidth + x;
					float vx = mesh.vertices[i * 3 + 0];
					float vz = mesh.vertices[i * 3 + 2];
					float dx = vx - impact.center.x;
					float dz = vz - impact.center.z;
					float dist = sqrt(dx * dx + dz * dz);

					float distFromRing = fabs(dist - impact.currentRadius);
					if (distFromRing < ringWidth) {
						affected++;
						float t = 1.0f - (distFromRing / ringWidth);
						float amount = impact.strength * t * t * deltaTime * 5.0f;

						float remaining = MAX_DEFORM - deformation[i];
						amount = fminf(fmaxf(amount, 0.0f), remaining);

						if (amount > 0.0001f) {
							deformation[i] += amount;
							mesh.vertices[i * 3 + 1] = Position.y - deformation[i];
							if (deformation[i] > maxDeform) maxDeform = deformation[i];
						}
					}
				}
			}
			printf("  vertices in ring: %d\n", affected);
		}

		impacts.erase(
			std::remove_if(impacts.begin(), impacts.end(),
				[](const ImpactEvent& e) { return !e.active; }),
			impacts.end()
		);

		updateDeformationColors();
		UpdateMeshBuffer(mesh, 0, mesh.vertices,
			mesh.vertexCount * 3 * sizeof(float), 0);
	}
	void applyDeformation(Vector3 impactPoint, float radius, float strength) {
		for (int z = 0; z < terrainDepth; z++) {
			for (int x = 0; x < terrainWidth; x++) {
				int i = z * terrainWidth + x;
				float vx = mesh.vertices[i * 3 + 0];
				float vz = mesh.vertices[i * 3 + 2];

				float dist = sqrt(pow(vx - impactPoint.x, 2) + pow(vz - impactPoint.z, 2));

				if (dist < radius) {
					float falloff = 1.0f - (dist / radius);
					falloff = falloff * falloff;
					float amount = strength * falloff;

					float remaining = MAX_DEFORM - deformation[i];
					amount = fminf(amount, remaining);

					if (amount > 0) {
						mesh.vertices[i * 3 + 1] -= amount;
						deformation[i] += amount;
						if (deformation[i] > maxDeform) maxDeform = deformation[i];
					}
				}
			}
		}
		updateDeformationColors();
		UpdateMeshBuffer(mesh, 0, mesh.vertices,
			mesh.vertexCount * 3 * sizeof(float), 0);
	}
	void updateDeformationColors() {
		float norm = maxDeform > 0.001f ? maxDeform : 1.0f;

		for (int i = 0; i < mesh.vertexCount; i++) {
			float t = fminf(deformation[i] / norm, 1.0f);
			mesh.colors[i * 4 + 0] = (unsigned char)(t * 255);  // R = deformation
			mesh.colors[i * 4 + 1] = 0;
			mesh.colors[i * 4 + 2] = 0;
			mesh.colors[i * 4 + 3] = 255;
		}

		// push color changes to GPU
		UpdateMeshBuffer(mesh, 3, mesh.colors,
			mesh.vertexCount * 4 * sizeof(unsigned char), 0);
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

	void virtual FillVertices() {
		for (int z = 0; z < terrainDepth; z++) {
			for (int x = 0; x < terrainWidth; x++) {
				int i = z * terrainWidth + x;

				mesh.vertices[i * 3 + 0] = (float)x + Position.x;
				mesh.vertices[i * 3 + 1] = Position.y;
				mesh.vertices[i * 3 + 2] = (float)z + Position.z;

				mesh.texcoords[i * 2 + 0] = (float)x / terrainWidth;
				mesh.texcoords[i * 2 + 1] = (float)z / terrainDepth;
			}
		}
	}

	void virtual FillIndices() {
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

	void virtual SetShader(Shader& s) {
		model.materials[0].shader = s;
		shader = s;
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
		DrawModel(model, { 0,0,0 }, 1.0f, WHITE);
	};
};

