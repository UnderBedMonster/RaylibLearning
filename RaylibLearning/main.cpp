#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>
#include "headers/Marble.h"
#include "headers/QuaternionR.h"
#include "headers/NoiseMap.h"
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>
#include <random>

#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION            100
#endif

#define M_PI 3.14159265358979323846

//drawing func of the XYZ axes
void DrawXYZLines() {

    DrawCubeV(Vector3{ 5.f, 0, 0 }, Vector3{ 10,0.1,0.1 }, Color(RED));
    DrawCubeV(Vector3{ 0, 5.f, 0 }, Vector3{ 0.1,10,0.1 }, Color(GREEN));//up
    DrawCubeV(Vector3{ 0, 0, 5.f }, Vector3{ 0.1,0.1,10 }, Color(BLUE));//left
}

Vector3 CatchMovement() {

    float velocity = 1.;

    if (GetKeyPressed() == KEY_BACKSPACE)
    {
        return Vector3{ 0,velocity,0 };
    }
    if (GetKeyPressed() == KEY_LEFT_CONTROL)
    {
        return Vector3{ 0,-velocity,0 };
    }
    if (GetKeyPressed() == KEY_A)
    {
        return Vector3{ 0,0,velocity };
    }
    if (GetKeyPressed() == KEY_S)
    {
        return Vector3{ velocity,0,0 };
    }
    if (GetKeyPressed() == KEY_D)
    {
        return Vector3{ 0,0,-velocity };
    }
    if (GetKeyPressed() == KEY_W)
    {
        return Vector3{ -velocity,0,0 };
    }
}

//degree to radians translation func 
float DegreetoRadians(float degree) {
    return (degree * (M_PI / 180.f));
}

struct Projectile {
    Vector3 position;
    Vector3 velocity;
    bool    active = true;
};

std::vector<Projectile> projectiles;

int main() {

    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "VODKA");
   
    //Objects
    Mesh CubeMesh = GenMeshCube(2.f, 2.f, 2.f);
    Mesh lightBulbMesh = GenMeshSphere(0.5,10,10);

    Vector3 MapPosition{0,0,0 };
    NoiseMap NM(MapPosition, 150,150,0.03,40);

    std::vector<Marble> marbles;
    marbles.reserve(10);
    Mesh marbleMesh = GenMeshSphere(2,10,10);

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<float> randFloat(0.0f, 135.0f);
    Vector3 position{};

    for (size_t i = 0; i < 10; i++)
    {
        position = { randFloat(gen),20,randFloat(gen) };
        marbles.emplace_back(position, marbleMesh, 2, 2, RED, false);
    }
    
    


    // Camera
    Camera camera = { 0 };
    camera.position = Vector3{ 80.0f, 100., 90.f };
    camera.target = Vector3{ 75.0f, -1.0f, 75.0f };
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    
    camera.projection = CAMERA_PERSPECTIVE;

    Vector3 lightPos = {60,60,60};

    //textures
    Texture2D texture = LoadTexture("textures/dora2.jpg");

    //shaders
    Shader shader = LoadShader(0, "shaders/lighting.frag");
    Shader TerrainShader = LoadShader("shaders/terrain.vert", "shaders/terrain.frag");

    for (size_t i = 0; i < marbles.size(); i++)
    {
        marbles[i].setTerrain(&NM);
        marbles[i].setTiling(1.f, 1.f);
        marbles[i].SetShader(shader);
    }

                    
    //o1.SetMaterialMapDiffuse(texture);
    NM.SetShader(TerrainShader);

    auto start = std::chrono::high_resolution_clock::now();

    DisableCursor();
    SetTargetFPS(120);

    while (!WindowShouldClose()) {
        UpdateCamera(&camera, CAMERA_FREE);

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto m_DeltadTime = std::chrono::duration<float>(currentTime - start).count();
        start = currentTime;
       
        for (size_t i = 0; i < marbles.size(); i++)
        {
            marbles[i].update(m_DeltadTime);
            
        }
        float speed = m_DeltadTime * 2;

        if (IsKeyPressed(KEY_K)) {
            Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            float   speed = 50.0f;

            Projectile p;
            p.position = camera.position;           // start at camera
            p.velocity = Vector3Scale(forward, speed); // shoot forward
            projectiles.push_back(p);
        }

        for (auto& p : projectiles) {
            p.position = Vector3Add(p.position, Vector3Scale(p.velocity, m_DeltadTime));

            // deactivate if too far
            if (Vector3Length(p.position) > 500.0f) p.active = false;
        }

        // remove inactive projectiles
        projectiles.erase(
            std::remove_if(projectiles.begin(), projectiles.end(),
                [](const Projectile& p) { return !p.active; }),
            projectiles.end()
        );

        //marble.debugPrint();

        BeginDrawing();
        ClearBackground(DARKGRAY);
          BeginMode3D(camera);

           NM.updateShader(lightPos);

           DrawXYZLines();
           DrawGrid(10, 1.0f);

           for (auto& p : projectiles) {
               DrawSphere(p.position, 0.3f, YELLOW);
           }
           
           for (size_t i = 0; i < marbles.size(); i++)
           {
               marbles[i].draw();
           }
           NM.draw();
           //NM.drawNormals(2.0f,10);
           DrawSphere(lightPos,0.5,YELLOW);

          EndMode3D();
        DrawFPS(10, 20);
        EndDrawing();
    }

    UnloadTexture(texture);
    CloseWindow();

    return 0;
}

