#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>
#include "PhysicalObj.h"
#include "QuaternionR.h"
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>




#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION            100
#endif

#define MAX_POINTS  11

#define M_PI 3.14159265358979323846




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
    for (int i = 0; i < vertices.size(); i+=3) {
        rlVertex2f(vertices[i].x, vertices[i].y);       // Center vertex (first point)
        rlVertex2f(vertices[i+1].x, vertices[i+1].y);         // Current vertex
        rlVertex2f(vertices[i + 2].x, vertices[i + 2].y);   // Next vertex
    }

    rlEnd();
}

Vector2 FindFigureCenter(std::vector<Vector2> &points)
{
    Vector2 output{ 0,0 };

    for (size_t i = 0; i < points.size(); i++)
    {
        output += points[i];
    }
    return (output / points.size());
}

void DrawPoly(std::vector<Vector2> points, Color color) {

    Vector2 center = FindFigureCenter(points);

    for (size_t i = 0; i < points.size() - 1; i++)
    {
        DrawTriangle(center, points[i], points[i + 1], color);
    }
}





Vector3 rotatePoint(const Vector3& point, const QuaternionR& q) {
    // Represent the point as a pure quaternion
    QuaternionR p{ 0, point.x, point.y, point.z };

    // Compute the rotated point: p' = q * p * q^-1
    QuaternionR rotated = q * p * q.conjugate();

    // Extract the rotated point
    return Vector3{ rotated.x, rotated.y, rotated.z };
}

float DegreetoRadians(float degree) {
    return (degree * (M_PI / 180.f));
}

int main() {

    const int screenWidth = 1280;
    const int screenHeight = 720;

    const int SidescreenWidth = 400;
    const int SidescreenHeight = 300;


    Vector3 axis{ 1, 1, 1 }; // Rotate around the Y-axis
    float angle = DegreetoRadians(1); // 90 degrees in radians

    QuaternionR q{ axis, angle };

    InitWindow(screenWidth, screenHeight, "VODKA");
    
    Mesh mesh = GenMeshCube(2.f, 2.f, 2.f);
    //Mesh mesh = GenMeshSphere(0.5,10,10);
    PhysicalObj o1(Vector3{1,20,1}, mesh, 0.1);
    //o1.BasicColision();


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
       
        for (int i = 0; i < o1.ObjMesh.vertexCount; i+=3)
        {    
            Vector3 p{
            o1.ObjMesh.vertices[i],
            o1.ObjMesh.vertices[i + 1],
            o1.ObjMesh.vertices[i + 2]
            };

            p = rotatePoint(p, q);

                o1.ObjMesh.vertices[  i  ] = p.x;
                o1.ObjMesh.vertices[i + 1] = p.y;
                o1.ObjMesh.vertices[i + 2] = p.z;
        }

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

        DrawRectangle(screenWidth-SidescreenWidth,0,SidescreenWidth,SidescreenHeight, WHITE);

        DrawPoly(o1.flattenX(), RED);

        EndDrawing();
    }

    UnloadTexture(texture);
    CloseWindow();

    return 0;
}