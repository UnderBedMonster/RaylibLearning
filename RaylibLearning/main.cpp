#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>
#include "headers/Marble.h"
#include "headers/Player.h"
#include "headers/QuaternionR.h"
#include "headers/NoiseMap.h"
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>
#include <external/glad.h>
#include <random>
#include <list>

// ---------------------------------------------------------------------------
// Scene tuning constants
// ---------------------------------------------------------------------------
constexpr int   SCREEN_WIDTH  = 1280;
constexpr int   SCREEN_HEIGHT = 720;

constexpr int   CAMERA_HEIGHT = 10;

// RPG-style pan camera: WASD slides it across the ground plane, height above
// terrain and the downward view angle stay fixed (no mouse-look, not tied to
// the Player entity) - think Diablo / Baldur's Gate rather than an FPS rig.
constexpr float CAMERA_PITCH_DEG     = 30.0f;  // downward tilt below horizontal
constexpr float CAMERA_YAW_DEG       = 45.0f;  // fixed facing direction
constexpr float CAMERA_PAN_SPEED     = 5.0f;  // world units / second
constexpr float CAMERA_LOOK_DISTANCE = 20.0f;

constexpr int   TERRAIN_WIDTH      = 150;
constexpr int   TERRAIN_DEPTH      = 150;
//constexpr float TERRAIN_NOISE_SCALE = 0.003f;
//constexpr float TERRAIN_AMPLITUDE   = 40.0f;

constexpr float LIGHT_INTENSITY = 1.1f;

constexpr int SHADOWMAP_RESOLUTION = 1024;

Shader terrainShader;
Shader lightShader;


// Draws a small red/green/blue gizmo along +X/+Y/+Z for orientation reference.
void DrawXYZLines() {
    DrawCubeV(Vector3{ 5.f, 0, 0 }, Vector3{ 10, 0.1f, 0.1f }, Color(RED));
    DrawCubeV(Vector3{ 0, 5.f, 0 }, Vector3{ 0.1f, 10, 0.1f }, Color(GREEN)); // up
    DrawCubeV(Vector3{ 0, 0, 5.f }, Vector3{ 0.1f, 0.1f, 10 }, Color(BLUE));  // forward
}

void DrawSceneForShadows(Player& player, Terrain& terrain, Shader depthShader)
{
    terrain.SetShader(depthShader);
	player.SetShader(depthShader);
    terrain.draw();
	player.draw();
    terrain.SetShader(terrainShader);
    player.SetShader(lightShader);
}


static RenderTexture2D LoadShadowCubemapRenderTexture(int size)
{
    RenderTexture2D target = { 0 };
    target.id = rlLoadFramebuffer();
    target.texture.width = size;
    target.texture.height = size;

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
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

// Pans `camera` across the XZ plane on WASD, re-pinning its height to
// CAMERA_HEIGHT above the terrain under it and re-aiming at a fixed
// pitch/yaw every frame, so the view angle never changes as it moves.
void UpdatePanCamera(Camera& camera, Terrain& terrain, float deltaTime)
{
    const float yaw   = CAMERA_YAW_DEG * DEG2RAD;
    const float pitch = -CAMERA_PITCH_DEG * DEG2RAD; // negative = looking down

    // Yaw-only basis (ignores pitch) so panning stays on the ground plane
    // regardless of how steeply the camera looks down.
    Vector3 forward = { sinf(yaw), 0.0f, cosf(yaw) };
    Vector3 right   = { cosf(yaw), 0.0f, -sinf(yaw) };

    Vector3 move = { 0.0f, 0.0f, 0.0f };
    if (IsKeyDown(KEY_W)) move = Vector3Add(move, forward);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forward);
    if (IsKeyDown(KEY_D)) move = Vector3Subtract(move, right);
    if (IsKeyDown(KEY_A)) move = Vector3Add(move, right);

    if (Vector3LengthSqr(move) > 0.0f) {
        move = Vector3Scale(Vector3Normalize(move), CAMERA_PAN_SPEED * deltaTime);
        camera.position.x += move.x;
        camera.position.z += move.z;
    }

    camera.position.y = terrain.getHeightAt(camera.position.x, camera.position.z) + CAMERA_HEIGHT;

    Vector3 lookDir = {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };
    camera.target = Vector3Add(camera.position, Vector3Scale(lookDir, CAMERA_LOOK_DISTANCE));
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "VODKA");

    // -----------------------------------------------------------------------
    // Terrain
    // -----------------------------------------------------------------------
    Vector3 terrainPosition{ 0, 0, 0 };
	Terrain terrain(terrainPosition, TERRAIN_WIDTH, TERRAIN_DEPTH);
    //NoiseMap terrain(terrainPosition, TERRAIN_WIDTH, TERRAIN_DEPTH, TERRAIN_NOISE_SCALE, TERRAIN_AMPLITUDE);
    terrain.Generate(); // must run after construction — see Terrain::Generate() for why
   
	
    std::random_device rd;
    std::mt19937 gen(rd());
    //std::uniform_real_distribution<float> randCoord(0.0f, MARBLE_SPAWN_RANGE);


    // -----------------------------------------------------------------------
    // Camera — a free-floating RPG-style pan camera (see UpdatePanCamera):
    // WASD slides it across the ground, height and view angle stay fixed.
    // -----------------------------------------------------------------------
    Camera camera = { 0 };
    camera.position   = Vector3{ 0.0f, 0.0f, 0.0f }; // Y is repinned by UpdatePanCamera every frame
	camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy       = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // -----------------------------------------------------------------------
    // Player: first-person body with an axis-aligned box collider (reuses
    // the same TerrainCollisionResponse ground-collision math as Marble's
    // sphere collider — see BoxColBox.h).
    // -----------------------------------------------------------------------
    Vector3 playerHalfExtents = { 0.4f, 0.9f, 0.4f }; // 0.8 x 1.8 x 0.8 box
	Model playerModel = LoadModel("models/player/source/girlWalking.glb");
    Vector3 playerSpawn = { 50,0,50 };
    Player player(playerSpawn, playerModel, 80.0f, playerHalfExtents, 1.6f, WHITE);
    player.setTerrain(&terrain);
	player.setScale(5.0f);
	player.rotation = QuaternionR(90.0f * DEG2RAD, Vector3{ 1.0f, 0.0f, 0.0f }); // Blender Z-up -> raylib Y-up correction

    constexpr const char* IMPORTED_MODEL_PATH_Pointer = "models/3d_lowpoly_arrow.glb";
    Model arrowPointerModel = LoadModel(IMPORTED_MODEL_PATH_Pointer);
    Vector3 arrowPointerVector = { 0.0f, 0.0f, 0.0f };

    //textures
    Texture2D texture = LoadTexture("textures/dora2.jpg");


    

    // Single point light shared by every lit shader in the scene (marbles,
    // imported model, terrain). Drawn as a yellow sphere so it's visible too.
    Vector3 lightPos = { 60, 10, 60 };
    float nearPlane = 0.1f, farPlane = 150.0f;
    Matrix shadowProj = MatrixPerspective(90.0f * DEG2RAD, 1.0f, nearPlane, farPlane);

    Vector3 targets[6] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };
    Vector3 ups[6] = {
        {0,-1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, {0,-1,0}, {0,-1,0}
    };

    // Loaded but not yet assigned to a material (marbles currently render
    // with a flat color) - wire it up with SetMaterialMapDiffuse() when needed.
    //Texture2D marbleTexture = LoadTexture("textures/dora2.jpg");

    // -----------------------------------------------------------------------
    // Shaders
    //   - `shader`: generic lit shader (Lambert + specular), used by marbles
    //     and the imported model.
    //   - `terrainShader`: bespoke shader that colors the terrain by height
    //     and computes its own normals via screen-space derivatives (the
    //     terrain mesh doesn't upload vertex normals).
    // Both read the same `lightPos` so everything is lit consistently.
    // -----------------------------------------------------------------------


    Shader pointDepthShader = LoadShader("shaders/shadowMapShader.vert", "shaders/shadowMapShader.frag");
    int depthLightPosLoc = GetShaderLocation(pointDepthShader, "lightPos");
    int depthFarPlaneLoc = GetShaderLocation(pointDepthShader, "farPlane");
    SetShaderValue(pointDepthShader, depthLightPosLoc, &lightPos, SHADER_UNIFORM_VEC3);
    SetShaderValue(pointDepthShader, depthFarPlaneLoc, &farPlane, SHADER_UNIFORM_FLOAT);


    lightShader = LoadShader("shaders/lighting.vert", "shaders/lighting.frag");
    terrainShader = LoadShader("shaders/terrain.vert", "shaders/terrain.frag");

    lightShader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(lightShader, "texture3");

    int lightPosLoc = GetShaderLocation(lightShader, "lightPos");
    int viewPosLoc  = GetShaderLocation(lightShader, "viewPos");
    SetShaderValue(lightShader, GetShaderLocation(lightShader, "lightIntensity"), &LIGHT_INTENSITY, SHADER_UNIFORM_FLOAT);

   

    terrain.lightIntensity = LIGHT_INTENSITY;
    terrain.SetShader(terrainShader);


    

    SetTargetFPS(120);

    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    RenderTexture2D shadowCubemap = LoadShadowCubemapRenderTexture(SHADOWMAP_RESOLUTION);

    SetShaderValue(lightShader, GetShaderLocation(lightShader, "farPlane"), &farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(terrainShader, GetShaderLocation(terrainShader, "farPlane"), &farPlane, SHADER_UNIFORM_FLOAT);

    // SetShaderValueTexture() assumes GL_TEXTURE_2D and can't bind a cubemap
    // (a GL texture's target is fixed at first bind), so bind the depth
    // cubemap to a fixed texture unit manually and point the sampler at it.
    constexpr int SHADOW_CUBEMAP_TEXTURE_UNIT = 1;
    rlActiveTextureSlot(SHADOW_CUBEMAP_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_CUBE_MAP, shadowCubemap.depth.id);
    rlActiveTextureSlot(0);

    int shadowUnit = SHADOW_CUBEMAP_TEXTURE_UNIT;
    SetShaderValue(lightShader, GetShaderLocation(lightShader, "shadowCubemap"), &shadowUnit, SHADER_UNIFORM_INT);
    SetShaderValue(terrainShader, GetShaderLocation(terrainShader, "shadowCubemap"), &shadowUnit, SHADER_UNIFORM_INT);

    while (!WindowShouldClose()) {
        // --- Update -----------------------------------------------------
        auto now = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            Vector2 mousePosition = GetMousePosition();
            Ray ray = GetScreenToWorldRay(mousePosition, camera);
            RayCollision rc = GetRayCollisionMesh(ray, terrain.mesh, terrain.model.transform);
            if (rc.hit)
            {
				arrowPointerVector = rc.point;
				std::cout << "Impact at: x=" << rc.point.x << ", y=" << rc.point.y << ", z=" << rc.point.z << std::endl;
            }
        }
        UpdatePanCamera(camera, terrain, deltaTime);

        // --- Shadow pass: re-render the depth cubemap every frame so it
        // stays in sync with a moving light and/or moving geometry. -------
        rlEnableFramebuffer(shadowCubemap.id);
        rlViewport(0, 0, SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION);
        rlEnableDepthTest();
        for (int i = 0; i < 6; i++)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, shadowCubemap.depth.id, 0);

            rlClearScreenBuffers(); // clears depth for this face
            Matrix shadowView = MatrixLookAt(lightPos, Vector3Add(lightPos, targets[i]), ups[i]);

            rlSetMatrixProjection(shadowProj);
            rlSetMatrixModelview(shadowView);

            DrawSceneForShadows(player,terrain, pointDepthShader);
        }
        rlDisableFramebuffer();
        rlViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT); // restore, otherwise the main scene renders at the wrong viewport

        // --- Draw ---------------------------------------------------------
        BeginDrawing();
        ClearBackground(DARKGRAY);
        BeginMode3D(camera);

            // Push the current light/camera position to every lit shader
            // before drawing anything that uses them this frame.
            terrain.updateShader(lightPos);
            SetShaderValue(lightShader, lightPosLoc, &lightPos, SHADER_UNIFORM_VEC3);
            SetShaderValue(lightShader, viewPosLoc, &camera.position, SHADER_UNIFORM_VEC3);

            DrawXYZLines();
            DrawGrid(10, 1.0f);

            terrain.draw();
            DrawSphere(lightPos, 0.5f, YELLOW);
            
			player.draw();
			DrawModel(arrowPointerModel, arrowPointerVector, 150.0f, WHITE);

        EndMode3D();
        DrawFPS(10, 20);
        EndDrawing();
    }
    CloseWindow();

    return 0;
}

// Load render texture for shadowmap projection
// NOTE: Load framebuffer with only a texture depth attachment,
// no color attachment required for shadowmap

