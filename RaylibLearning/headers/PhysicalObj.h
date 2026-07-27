#pragma once

#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <chrono>
#include <vector>
#include "QuaternionR.h"

constexpr static float G_EARTH = 9.80665f;

class PhysicalObj
{
public:

	Vector3 Position;
	Vector3 Direction = { 1.0f, 0.0f, 0.0f };
	Model ObjModel;
	int ObjModelIndex = 0;
	int ObjMaterialSetIndex =0;
	float ModelScale = 20.0f;
	float ObjMass;
	Vector3 Velocity;
	QuaternionR rotation;
	Color color = RED;

	bool inCollisionWithterrain = false;
	
	float tiling[2] = { 1.0f, 1.0f };
	int tilingLocation;
	int textureLocation;

	PhysicalObj(Vector3 pos, Model& model, float mass) {
		Position = pos;
		ObjModel = model;
		ObjMass = mass;
		Velocity = { 0.f, 0.f, 0.f };
	}

	~PhysicalObj(){}
	
	void setTiling(float i, float j) {
		tiling[0] = i;
		tiling[1] = j;
		SetShaderValue(ObjModel.materials[ObjMaterialSetIndex].shader, tilingLocation, tiling, SHADER_UNIFORM_VEC2);
	}
	void setScale(float scale) {
		ModelScale = scale;
	}
	void printvertices() {
		for (int i = 0; i < ObjModel.meshes[ObjModelIndex].vertexCount; i++)
		{
		std::cout 
			<< "Point " << i << ": "  
			<< "x:" << ObjModel.meshes[ObjModelIndex].vertices[i * 3]
			<< " y:" << ObjModel.meshes[ObjModelIndex].vertices[i * 3 + 1]
			<< " z:" << ObjModel.meshes[ObjModelIndex].vertices[i * 3 + 2]
			<< std::endl;
		}
	}

	void changeDirection(Vector3 newDirection) {
		Direction = newDirection;
	}

	void Gravity(float deltaTime)
	{
		if (!inCollisionWithterrain)
		{
			AddVelocityObj(Vector3{ 0.f, -((G_EARTH) * deltaTime), 0.f });
		}
	}

	void ChangeObjPos(Vector3 NewPos)
	{
		Position = NewPos;
	}

	void AddVelocityObj(Vector3 velocity)
	{
		Velocity += velocity;
	}

	void ApplyForceObj(Vector3 direction, float jouls)
	{
		Velocity += (direction*jouls);
	}

	void virtual update(float deltaTime) {
		//std::cout << "x: " << Velocity.x << " y: " << Velocity.y << " z: " << Velocity.z << std::endl;
		Gravity(deltaTime);
		Position = Position + (Velocity * deltaTime); // Update position based on velocity and deltaTime
		rotation.normalize();
	}

	void SetMaterialMapDiffuse(Texture2D texture) {
		ObjModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
	}

	void SetShader(Shader shader) {
		for (size_t i = 0; i < ObjModel.materialCount; i++)
		{
		ObjModel.materials[i].shader = shader;
		tilingLocation = GetShaderLocation(ObjModel.materials[i].shader, "tiling");
		textureLocation = GetShaderLocation(ObjModel.materials[i].shader, "texture0");
		SetShaderValue(ObjModel.materials[i].shader, tilingLocation, tiling, SHADER_UNIFORM_VEC2);
		}
	}

	Shader getShader() {
		return ObjModel.materials[0].shader;
	}

	void draw()
	{	
		ObjModel.transform = rotation.toMatrix();
		DrawModel(ObjModel, Position, ModelScale, color);
	};
};

