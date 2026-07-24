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
#include <external/glad.h>
#include <random>

// ---------------------------------------------------------------------------
// Scene tuning constants
// ---------------------------------------------------------------------------
constexpr int   SCREEN_WIDTH  = 1280;
constexpr int   SCREEN_HEIGHT = 720;

constexpr int   TERRAIN_WIDTH      = 150;
constexpr int   TERRAIN_DEPTH      = 150;
constexpr float TERRAIN_NOISE_SCALE = 0.03f;
constexpr float TERRAIN_AMPLITUDE   = 40.0f;

constexpr int   MARBLE_COUNT       = 0;
constexpr float MARBLE_RADIUS      = 2.0f;
constexpr float MARBLE_MASS        = 2.0f;
constexpr float MARBLE_SPAWN_HEIGHT = 20.0f;
constexpr float MARBLE_SPAWN_RANGE  = 135.0f; // marbles spawn at random X/Z in [0, this)

constexpr float PROJECTILE_SPEED     = 50.0f;
constexpr float PROJECTILE_MAX_RANGE = 500.0f; // despawn once this far from the origin

constexpr float LIGHT_INTENSITY = 5.0f;

constexpr int SHADOWMAP_RESOLUTION = 1024;

// Foreign mesh (glTF binary) rendered alongside the procedural terrain/marbles.
constexpr const char* IMPORTED_MODEL_PATH = "models/ManiquiMozillaLQ.glb";

struct Projectile {
    Vector3 position;
    Vector3 velocity;
    bool    active = true;
};

// Draws a small red/green/blue gizmo along +X/+Y/+Z for orientation reference.
void DrawXYZLines() {
    DrawCubeV(Vector3{ 5.f, 0, 0 }, Vector3{ 10, 0.1f, 0.1f }, Color(RED));
    DrawCubeV(Vector3{ 0, 5.f, 0 }, Vector3{ 0.1f, 10, 0.1f }, Color(GREEN)); // up
    DrawCubeV(Vector3{ 0, 0, 5.f }, Vector3{ 0.1f, 0.1f, 10 }, Color(BLUE));  // forward
}


static RenderTexture2D LoadShadowmapRenderTexture(int width, int height);
static void UnloadShadowmapRenderTexture(RenderTexture2D target);
static void DrawScene(Model cube, Model robot);

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "VODKA");

    // -----------------------------------------------------------------------
    // Terrain
    // -----------------------------------------------------------------------
    Vector3 terrainPosition{ 0, 0, 0 };
    NoiseMap terrain(terrainPosition, TERRAIN_WIDTH, TERRAIN_DEPTH, TERRAIN_NOISE_SCALE, TERRAIN_AMPLITUDE);

    // -----------------------------------------------------------------------
    // Marbles: randomly scattered spheres that fall and collide with the terrain
    // -----------------------------------------------------------------------
    std::vector<Marble> marbles;
    marbles.reserve(MARBLE_COUNT);
    Mesh marbleMesh = GenMeshSphere(MARBLE_RADIUS, 10, 10);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> randCoord(0.0f, MARBLE_SPAWN_RANGE);

    for (int i = 0; i < MARBLE_COUNT; i++)
    {
        Vector3 spawnPos = { randCoord(gen), MARBLE_SPAWN_HEIGHT, randCoord(gen) };
        marbles.emplace_back(spawnPos, marbleMesh, MARBLE_MASS, MARBLE_RADIUS, RED, false);
    }

    // -----------------------------------------------------------------------
    // Camera
    // -----------------------------------------------------------------------
    Camera camera = { 0 };
    camera.position   = Vector3{ 80.0f, 100.0f, 90.0f };
    camera.target     = Vector3{ 75.0f, -1.0f, 75.0f };
    camera.up         = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy       = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Single point light shared by every lit shader in the scene (marbles,
    // imported model, terrain). Drawn as a yellow sphere so it's visible too.
    Vector3 lightPos = { 60, 10, 60 };
    float nearPlane = 0.1f, farPlane = 50.0f;
    Matrix shadowProj = MatrixPerspective(90.0f * DEG2RAD, 1.0f, nearPlane, farPlane);

    Vector3 targets[6] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };
    Vector3 ups[6] = {
        {0,-1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, {0,-1,0}, {0,-1,0}
    };

    rlEnableFramebuffer(shadowCubemap.id);
    for (int i = 0; i < 6; i++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, shadowCubemap.depth.id, 0);

        rlClearScreenBuffers(); // clears depth for this face
        Matrix shadowView = MatrixLookAt(lightPos, Vector3Add(lightPos, targets[i]), ups[i]);

        rlSetMatrixProjection(shadowProj);
        rlSetMatrixModelview(shadowView);

        DrawSceneForShadows(); // your draw calls, using the pointDepthShader below
    }
    rlDisableFramebuffer();

    // Loaded but not yet assigned to a material (marbles currently render
    // with a flat color) - wire it up with SetMaterialMapDiffuse() when needed.
    Texture2D marbleTexture = LoadTexture("textures/dora2.jpg");

    // -----------------------------------------------------------------------
    // Shaders
    //   - `shader`: generic lit shader (Lambert + specular), used by marbles
    //     and the imported model.
    //   - `terrainShader`: bespoke shader that colors the terrain by height
    //     and computes its own normals via screen-space derivatives (the
    //     terrain mesh doesn't upload vertex normals).
    // Both read the same `lightPos` so everything is lit consistently.
    // -----------------------------------------------------------------------
    Shader shader        = LoadShader("shaders/lighting.vert", "shaders/lighting.frag");
    Shader terrainShader = LoadShader("shaders/terrain.vert", "shaders/terrain.frag");

    int lightPosLoc = GetShaderLocation(shader, "lightPos");
    int viewPosLoc  = GetShaderLocation(shader, "viewPos");
    SetShaderValue(shader, GetShaderLocation(shader, "lightIntensity"), &LIGHT_INTENSITY, SHADER_UNIFORM_FLOAT);

    for (auto& marble : marbles)
    {
        marble.setTerrain(&terrain);
        marble.setTiling(1.0f, 1.0f);
        marble.SetShader(shader);
    }

    terrain.SetShader(terrainShader);

    // -----------------------------------------------------------------------
    // Imported mesh: loaded from disk (glTF/OBJ/etc, anything raylib's
    // LoadModel supports) rather than generated procedurally like the
    // marbles/terrain above. Shares the same lit shader as the marbles.
    // -----------------------------------------------------------------------
    Model importedModel = LoadModel(IMPORTED_MODEL_PATH);
    for (int i = 0; i < importedModel.materialCount; i++)
    {
        importedModel.materials[i].shader = shader;
    }
    Vector3 importedModelPosition = { 75.0f, terrain.getHeightAt(75.0f, 75.0f), 75.0f };

    std::vector<Projectile> projectiles;

    DisableCursor();
    SetTargetFPS(120);

    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    while (!WindowShouldClose()) {
        // --- Update -----------------------------------------------------
        UpdateCamera(&camera, CAMERA_FREE);

        auto now = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        for (auto& marble : marbles)
        {
            marble.update(deltaTime);
        }

        // Fire a projectile from the camera, in the direction it's facing.
        if (IsKeyPressed(KEY_K)) {
            Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));

            Projectile p;
            p.position = camera.position;
            p.velocity = Vector3Scale(forward, PROJECTILE_SPEED);
            projectiles.push_back(p);
        }

        for (auto& p : projectiles) {
            p.position = Vector3Add(p.position, Vector3Scale(p.velocity, deltaTime));
            if (Vector3Length(p.position) > PROJECTILE_MAX_RANGE) p.active = false;
        }

        projectiles.erase(
            std::remove_if(projectiles.begin(), projectiles.end(),
                [](const Projectile& p) { return !p.active; }),
            projectiles.end()
        );

        // --- Draw ---------------------------------------------------------
        BeginDrawing();
        ClearBackground(DARKGRAY);
        BeginMode3D(camera);

            // Push the current light/camera position to every lit shader
            // before drawing anything that uses them this frame.
            terrain.updateShader(lightPos);
            SetShaderValue(shader, lightPosLoc, &lightPos, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, viewPosLoc, &camera.position, SHADER_UNIFORM_VEC3);

            DrawXYZLines();
            DrawGrid(10, 1.0f);

            DrawModel(importedModel, importedModelPosition, 1.0f, WHITE);

            for (auto& p : projectiles) {
                DrawSphere(p.position, 0.3f, YELLOW);
            }

            for (auto& marble : marbles)
            {
                marble.draw();
            }

            terrain.draw();
            DrawSphere(lightPos, 0.5f, YELLOW);

        EndMode3D();
        DrawFPS(10, 20);
        EndDrawing();
    }

    UnloadTexture(marbleTexture);
    UnloadModel(importedModel);
    CloseWindow();

    return 0;
}

// Load render texture for shadowmap projection
// NOTE: Load framebuffer with only a texture depth attachment,
// no color attachment required for shadowmap

static RenderTexture2D LoadShadowCubemapRenderTexture(int size)
{
    RenderTexture2D target = { 0 };
    target.id = rlLoadFramebuffer();
    target.texture.width = size;
    target.texture.height = size;

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);

        // Create a depth CUBEMAP instead of a single depth texture
        unsigned int depthCubemapId = 0;
        glGenTextures(1, &depthCubemapId);
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemapId);

        for (int i = 0; i < 6; i++)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                         size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        target.depth.id = depthCubemapId;
        target.depth.width = size;
        target.depth.height = size;
        target.depth.mipmaps = 1;

        rlDisableFramebuffer();
    }

    return target;
}
// Unload shadowmap render texture from GPU memory (VRAM)
static void UnloadShadowmapRenderTexture(RenderTexture2D target)
{
    if (target.id > 0)
    {
        // NOTE: Depth texture/renderbuffer is automatically
        // queried and deleted before deleting framebuffer
        rlUnloadFramebuffer(target.id);
    }
}