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
	Mesh ObjMesh;
	std::vector<Vector3> objVertices{0};
	Model Model;
	float ObjMass;
	Vector3 Velocity;
	QuaternionR rotation;
	
	float tiling[2] = { 1.0f, 1.0f };
	int tilingLocation;
	int textureLocation;

	PhysicalObj(Vector3 pos, Mesh objmesh, float mass) {
		Position = pos;
		ObjMesh = objmesh;
		Model = LoadModelFromMesh(ObjMesh);
		ObjMass = mass;
		Velocity = { 0.f, 0.f, 0.f }; 
		makeVertices();
	}

	~PhysicalObj(){}
	
	void setTiling(float i, float j) {
		tiling[0] = i;
		tiling[1] = j;
		SetShaderValue(Model.materials[0].shader, tilingLocation, tiling, SHADER_UNIFORM_VEC2);
	}

	void updateModel() {
		Model = LoadModelFromMesh(ObjMesh);
	}

	void updateMesh() {
		int meshPointer = 0;
		for (size_t i = 0; i < objVertices.size(); i++)
		{
			ObjMesh.vertices[meshPointer++] = objVertices[i].x;
			ObjMesh.vertices[meshPointer++] = objVertices[i].y;
			ObjMesh.vertices[meshPointer++] = objVertices[i].z;
		}
	}

	void makeVertices() {
		for (size_t i = 0; i < ObjMesh.vertexCount; i+=3)
		{
			objVertices.push_back(Vector3{ObjMesh.vertices[i], ObjMesh.vertices[i+1], ObjMesh.vertices[i+2]});
		}
	}

	void printvertices() {
		for (int i = 0; i < objVertices.size(); i++)
		{
		std::cout 
			<< "Point " << i << ": "  
			<< "x:" << objVertices[i].x
			<< " y:" << objVertices[i].y 
			<< " z:" << objVertices[i].z 
			<< std::endl;
		}
	}

	void Gravity(float deltaTime)
	{
		if (Position.y > 0.f)
		{
		AddVelocityObj(Vector3{ 0.f, -((G_EARTH) * deltaTime), 0.f });
		}
		else
		{
			Position.y = 0.f;
			Velocity.y = 0.f;
		}
	}

	bool BasicColision(){
		for (size_t i = 0; i < ObjMesh.vertexCount; i++)
		{
		    std::cout << "Vert " << i << " ";
			for (size_t j = 0; j < 3; j++)
			{
			    std::cout << ObjMesh.vertices[j] << " ; ";
			}
	      	std::cout << std::endl;
		}
		return true;
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

	void update(float deltaTime) {
		//std::cout << "x: " << Velocity.x << " y: " << Velocity.y << " z: " << Velocity.z << std::endl;
		Position = Position + (Velocity * deltaTime); // Update position based on velocity and deltaTime
		rotation.normalize();
		Gravity(deltaTime);
	}

	void SetMaterialMapDiffuse(Texture2D texture) {
		Model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
	}

	void SetShader(Shader shader) {
		Model.materials[0].shader = shader;
		tilingLocation = GetShaderLocation(Model.materials[0].shader, "tiling");
		textureLocation = GetShaderLocation(Model.materials[0].shader, "texture0");
		SetShaderValue(Model.materials[0].shader, tilingLocation, tiling, SHADER_UNIFORM_VEC2);
	}

	void draw()
	{	
		Model.transform = rotation.toMatrix();
		DrawModel(Model, Position, 1.0f, WHITE);
	};
};

