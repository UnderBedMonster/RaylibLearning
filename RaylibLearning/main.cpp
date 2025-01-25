#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>
#include "PhysicalObj.h"
#include <chrono>
#include <thread>


#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION            100
#endif







void DrawXYZLines() {

    DrawCubeV(Vector3{ 5.f, 0, 0 }, Vector3{ 10,0.1,0.1 }, Color(RED));
    DrawCubeV(Vector3{ 0, 5.f, 0 }, Vector3{ 0.1,10,0.1 }, Color(GREEN));
    DrawCubeV(Vector3{ 0, 0, 5.f }, Vector3{ 0.1,0.1,10 }, Color(BLUE));
}


int main() {

    const int screenWidth = 1920;
    const int screenHeight = 1080;

    InitWindow(screenWidth, screenHeight, "Raylib - Cube Reflections (Simulated)");
    
    Mesh mesh = GenMeshCube(1.f, 1.f, 1.f);
    PhysicalObj o1(Vector3{1,20,1}, mesh, 1);


    // Define the camera to look into our 3d world
    Camera camera = { 0 };
    camera.position = Vector3{ 50.0f, 5.0f, 0.0f };
    camera.target = Vector3{ 0.0f, 0.0f, 0.0f };
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Light properties (simulated)
    Vector3 lightPosition = { 0.0f, 0.0f, 0.0f }; // Position of the "light"
    Color lightColor = WHITE;

    // Load a texture and assign to cube model
    Texture2D texture = LoadTexture("textures/dora2.jpg");
    o1.SetMaterialMapDiffuse(texture);

    float tiling[2] = { 1.0f, 1.0f };
    Shader shader = LoadShader(0, TextFormat("shaders/lighting.frag", GLSL_VERSION));
    SetShaderValue(shader, GetShaderLocation(shader, "tiling"), tiling, SHADER_UNIFORM_VEC2);
    o1.SetShader(shader);

    DisableCursor();

    auto start = std::chrono::high_resolution_clock::now();

    o1.AddVelocityObj(Vector3{0,0,0});

    SetTargetFPS(200);

    while (!WindowShouldClose()) {
        UpdateCamera(&camera, CAMERA_FREE);

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto m_DeltadTime = std::chrono::duration<float>(currentTime - start).count();
        start = currentTime;

        o1.update(m_DeltadTime);

        BeginDrawing();

        ClearBackground(DARKGRAY);

        BeginMode3D(camera);

        DrawXYZLines();

        DrawGrid(10, 1.0f);

        BeginShaderMode(shader);

        o1.draw();

        EndShaderMode();

        EndMode3D();

        DrawFPS(10, 20);
        EndDrawing();
    }

    UnloadTexture(texture);
    CloseWindow();

    return 0;
}