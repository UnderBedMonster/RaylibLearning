#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>
#include "PhysicalObj.h"
#include <chrono>
#include <thread>
#include <vector>



#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION            100
#endif

#define MAX_POINTS  11





void DrawXYZLines() {

    DrawCubeV(Vector3{ 5.f, 0, 0 }, Vector3{ 10,0.1,0.1 }, Color(RED));
    DrawCubeV(Vector3{ 0, 5.f, 0 }, Vector3{ 0.1,10,0.1 }, Color(GREEN));
    DrawCubeV(Vector3{ 0, 0, 5.f }, Vector3{ 0.1,0.1,10 }, Color(BLUE));
}

void DrawTexturePoly(std::vector<Vector2> vertices, Color color) {
    if (vertices.size() < 3) return; // Need at least 3 points to draw a polygon

    rlBegin(RL_QUADS); // Use triangles for filled polygon

    rlColor4ub(color.r, color.g, color.b, color.a);

    // Fan triangulation (works well for convex polygons)
    for (int i = 1; i < vertices.size() - 1; i++) {
        rlVertex2f(vertices[0].x, vertices[0].y);       // Center vertex (first point)
        rlVertex2f(vertices[i].x, vertices[i].y);         // Current vertex
        rlVertex2f(vertices[i + 1].x, vertices[i + 1].y);   // Next vertex
    }

    rlEnd();
}

void DrawPolygon(const std::vector<Vector2>& vertices, Color color) {
    if (vertices.size() < 3) return;

    for (size_t i = 1; i < vertices.size() - 1; ++i) {
        DrawTriangle(vertices[0], vertices[i], vertices[i + 1], color);
    }
}

   

    std::vector<Vector2> GenerateRectangleVertices(float width, float height) {
        std::vector<Vector2> vertices{3}; // 4 vertices for a rectangle

    vertices[0] = { 0, 0 }; // Top-left
    vertices[1] = { 50, 0 };  // Top-right
    vertices[2] = { 50, 50 };   // Bottom-right
    

    return vertices;
}



int main() {

    const int screenWidth = 1280;
    const int screenHeight = 720;

    const int SidescreenWidth = 400;
    const int SidescreenHeight = 300;


    InitWindow(screenWidth, screenHeight, "VODKA");
    
    Mesh mesh = GenMeshCube(1.f, 1.f, 1.f);
    //Mesh mesh = GenMeshSphere(0.5,10,10);
    PhysicalObj o1(Vector3{1,20,1}, mesh, 0.1);
    o1.BasicColision();


    // Define the camera to look into our 3d world
    Camera camera = { 0 };
    camera.position = Vector3{ 50.0f, 5.0f, 0.0f };
    camera.target = Vector3{ 0.0f, 0.0f, 0.0f };
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    //2d sidecreen for side perspective objects visualization
    Camera2D CameraSideScreen = { 0 };
    CameraSideScreen.offset = Vector2{ SidescreenWidth / 2.0f, SidescreenHeight / 2.0f };
    ClearBackground(WHITE);
    RenderTexture SideScreen = LoadRenderTexture(150, 150);


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


        //BeginMode2D(CameraSideScreen);
        
        //DrawTexturePoly(GenerateRectangleVertices(150,150), BLACK);

        DrawPolygon(GenerateRectangleVertices(150, 150), BLACK);

        //DrawRectangle(screenWidth-SidescreenWidth, 0, SidescreenWidth, SidescreenHeight, WHITE);

        //EndMode2D();

        EndDrawing();
    }

    UnloadTexture(texture);
    CloseWindow();

    return 0;
}