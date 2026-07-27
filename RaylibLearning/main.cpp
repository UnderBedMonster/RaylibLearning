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

// ---------------------------------------------------------------------------
// Scene tuning constants
// ---------------------------------------------------------------------------
constexpr int   SCREEN_WIDTH  = 1280;
constexpr int   SCREEN_HEIGHT = 720;

constexpr int   TERRAIN_WIDTH      = 150;
constexpr int   TERRAIN_DEPTH      = 150;
constexpr float TERRAIN_NOISE_SCALE = 0.003f;
constexpr float TERRAIN_AMPLITUDE   = 40.0f;

constexpr int   MARBLE_COUNT       = 0;
constexpr float MARBLE_RADIUS      = 2.0f;
constexpr float MARBLE_MASS        = 2.0f;
constexpr float MARBLE_SPAWN_HEIGHT = 20.0f;
constexpr float MARBLE_SPAWN_RANGE  = 135.0f; // marbles spawn at random X/Z in [0, this)

constexpr float PROJECTILE_SPEED     = 50.0f;
constexpr float PROJECTILE_MAX_RANGE = 500.0f; // despawn once this far from the origin

constexpr float LIGHT_INTENSITY = 1.1f;

constexpr int SHADOWMAP_RESOLUTION = 1024;

// girlmodel's animation clip framerate. raylib's ModelAnimation doesn't
// store an authoritative fps - Mixamo bakes clips at 30fps and Blender's
// glTF exporter samples at the scene's frame rate (30 by default), so 30 is
// the right assumption unless you've changed that in Blender's export
// settings. Which CLIP plays is runtime state (girlCurrentAnim in main()),
// not a constant - see PlayGirlAnimation() below.
constexpr float GIRL_ANIM_FPS = 30.0f;

Vector3 importedModelPosition;
Vector3 girlModelPosition;

// Foreign mesh (glTF binary) rendered alongside the procedural terrain/marbles.
constexpr const char* IMPORTED_MODEL_PATH = "models/ManiquiMozillaLQ.glb";

// girlmodel: switched from the glTF export (one_one.glb) to IQM
// (girlWalk.iqm) - raylib's glTF skinning path was mixing world-space bind
// pose against local-space animated pose whenever a non-joint ancestor node
// (Blender's "Armature" object, with the compensating scale/rotation FBX
// import adds for Mixamo's cm/Z-up rig) sat above the skeleton, which
// silently produced garbage bone transforms. IQM's bone hierarchy has no
// such ancestor-node concept, sidestepping that whole bug class. Unlike the
// glTF file, IQM doesn't embed texture images - textures get wired up
// manually in C++ instead (see EnsureFallbackMaps and the material setup
// below).
constexpr const char* GIRL_MODEL_PATH = "models/girlWalk.iqm";

// DrawModel's scale factor for girlmodel - her raw mesh is authored at
// roughly real human size (~1.7 units tall, matching the scene's other
// units), so 1.0 renders her life-sized. Turn this down to shrink her.
constexpr float GIRL_MODEL_SCALE = 0.01f;

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

// Makes a 1x1 texture of a solid color. Used to give every lit material a
// "neutral" normal/roughness map so `shader`'s texture2/texture3 samplers
// always have something valid to read - see EnsureFallbackMaps() below for
// why that matters.
static Texture2D MakeSolidTexture(Color color)
{
    Image img = GenImageColor(1, 1, color);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

// raylib only binds a material map's texture to its GL texture unit when
// that map actually HAS a texture (see rmodels.c's DrawMesh: `if
// (material.maps[i].texture.id > 0) { bind it }`). If a material has no
// normal/roughness map, that texture unit is simply left as whatever the
// PREVIOUS mesh drawn this frame put there - so without this, one object's
// normal map could "leak" onto the next object drawn right after it. Call
// this once per material (after assigning `shader`) to guarantee
// texture2/texture3 always hold a harmless, known value instead.
static void EnsureFallbackMaps(Material& material, Texture2D flatNormal, Texture2D flatRoughness)
{
    if (material.maps[MATERIAL_MAP_NORMAL].texture.id == 0)
        material.maps[MATERIAL_MAP_NORMAL].texture = flatNormal;
    if (material.maps[MATERIAL_MAP_ROUGHNESS].texture.id == 0)
        material.maps[MATERIAL_MAP_ROUGHNESS].texture = flatRoughness;
}

// Switches girlmodel to a different animation clip as an "instant cut" -
// snaps straight to the new clip's first frame, no crossfade/blend with
// whatever pose she was in. `currentAnim`/`animTime` are the two bits of
// state this needs to change; passed by reference since they're locals
// living in main(), not members of some AnimationPlayer object (there's
// only ever one girlmodel, so a whole class for this would just be
// indirection with nothing to abstract over).
static void PlayGirlAnimation(int newIndex, int animCount, int& currentAnim, float& animTime)
{
    if (newIndex < 0 || newIndex >= animCount || newIndex == currentAnim) return;
    currentAnim = newIndex;
    animTime = 0.0f; // restart the new clip from its first frame
}

void DrawSceneForShadows(Model& importedModel, Model& girlModel, std::vector<Marble>& marbles, Terrain& terrain, Shader depthShader)
{
    // Same fix as the main color shader: this function runs once per
    // shadow-cubemap face (6x/frame), and girlModel's draw at the bottom
    // uploads her REAL bone matrices into depthShader's shared boneMatrices
    // uniform - contaminating it for terrain/marbles/importedModel next time
    // this function runs, since none of them are actually rigged. Stamping
    // it back to identity here, before anything else draws, undoes that.
    SetShaderValueMatrix(depthShader, GetShaderLocation(depthShader, "boneMatrices"), MatrixIdentity());

    Shader originalTerrainShader = terrain.getShader(); // if you have a getter, or store it separately
    terrain.SetShader(depthShader);
    terrain.draw();
    terrain.SetShader(originalTerrainShader);

    for (auto& marble : marbles) {
        Shader original = marble.getShader();
        marble.SetShader(depthShader);
        marble.draw();
        marble.SetShader(original);
    }

    for (int i = 0; i < importedModel.materialCount; i++) {
        Shader original = importedModel.materials[i].shader;
        importedModel.materials[i].shader = depthShader;
        DrawModel(importedModel, importedModelPosition, 1.0f, WHITE);
        importedModel.materials[i].shader = original;
    }

    // girlmodel's materials are authored `doubleSided` in the glTF (thin
    // hair-card/cloth-shell geometry with only one layer of triangles), so
    // backface culling has to be off while she's drawn here too - otherwise
    // her shadow would have holes matching whichever side got culled.
    std::vector<Shader> girlOriginalShaders(girlModel.materialCount);
    for (int i = 0; i < girlModel.materialCount; i++) {
        girlOriginalShaders[i] = girlModel.materials[i].shader;
        girlModel.materials[i].shader = depthShader;
    }
    rlDisableBackfaceCulling();
    DrawModel(girlModel, girlModelPosition, GIRL_MODEL_SCALE, WHITE);
    rlEnableBackfaceCulling();
    for (int i = 0; i < girlModel.materialCount; i++) {
        girlModel.materials[i].shader = girlOriginalShaders[i];
    }
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

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "VODKA");

    // -----------------------------------------------------------------------
    // Terrain
    // -----------------------------------------------------------------------
    Vector3 terrainPosition{ 0, 0, 0 };
    NoiseMap terrain(terrainPosition, TERRAIN_WIDTH, TERRAIN_DEPTH, TERRAIN_NOISE_SCALE, TERRAIN_AMPLITUDE);
    //Terrain terrain(terrainPosition, TERRAIN_WIDTH, TERRAIN_DEPTH); // swap in for a flat terrain instead
    terrain.Generate(); // must run after construction — see Terrain::Generate() for why
    // -----------------------------------------------------------------------
    // Marbles: randomly scattered spheres that fall and collide with the terrain
    // -----------------------------------------------------------------------
    std::vector<Marble> marbles;
    marbles.reserve(MARBLE_COUNT);
    Mesh marbleMesh = GenMeshSphere(MARBLE_RADIUS, 10, 10);
	Mesh terrainMesh = GenMeshPlane(TERRAIN_WIDTH, TERRAIN_DEPTH, TERRAIN_WIDTH - 1, TERRAIN_DEPTH - 1);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> randCoord(0.0f, MARBLE_SPAWN_RANGE);

    for (int i = 0; i < MARBLE_COUNT; i++)
    {
        Vector3 spawnPos = { randCoord(gen), MARBLE_SPAWN_HEIGHT, randCoord(gen) };
        marbles.emplace_back(spawnPos, marbleMesh, MARBLE_MASS, MARBLE_RADIUS, RED, false);
    }

    // -----------------------------------------------------------------------
    // Camera — position/target are driven by the player every frame (see the
    // Update section in the main loop below), not by raylib's free camera.
    // -----------------------------------------------------------------------
    Camera camera = { 0 };
    camera.up         = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy       = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // -----------------------------------------------------------------------
    // Player: first-person body with an axis-aligned box collider (reuses
    // the same TerrainCollisionResponse ground-collision math as Marble's
    // sphere collider — see BoxColBox.h).
    // -----------------------------------------------------------------------
    Vector3 playerHalfExtents = { 0.4f, 0.9f, 0.4f }; // 0.8 x 1.8 x 0.8 box
    Mesh playerMesh = GenMeshCube(playerHalfExtents.x * 2.0f, playerHalfExtents.y * 2.0f, playerHalfExtents.z * 2.0f);
    Vector3 playerSpawn = { 75.0f, terrain.getHeightAt(75.0f, 75.0f) + 5.0f, 75.0f };
    Player player(playerSpawn, playerMesh, 80.0f, playerHalfExtents, 1.6f, BLUE);
    player.setTerrain(&terrain);

    
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
    // Same raylib default-vertex-attribute-value bug as `shader` - see its
    // setup comment above for the full explanation. Without this, every
    // unrigged mesh drawn through this shader (terrain, marbles, the
    // mannequin) would get "skinned" by boneMatrices[0], which starts life
    // as a zero matrix and collapses them to a single point.
    SetShaderValueMatrix(pointDepthShader, GetShaderLocation(pointDepthShader, "boneMatrices"), MatrixIdentity());


    Shader shader        = LoadShader("shaders/lighting.vert", "shaders/lighting.frag");
    Shader terrainShader = LoadShader("shaders/terrain.vert", "shaders/terrain.frag");

    int lightPosLoc = GetShaderLocation(shader, "lightPos");
    int viewPosLoc  = GetShaderLocation(shader, "viewPos");
    SetShaderValue(shader, GetShaderLocation(shader, "lightIntensity"), &LIGHT_INTENSITY, SHADER_UNIFORM_FLOAT);

    // LoadShader() only auto-wires locs[] for texture0/1/2 (albedo/metalness/
    // normal - see LoadShaderFromMemory in rcore.c). lighting.frag also reads
    // a 4th map, roughness (MATERIAL_MAP_ROUGHNESS = GL texture unit 3), and
    // raylib has no built-in name for a 4th slot - without this line the
    // "texture3" sampler would default to unit 0 and silently read the
    // albedo texture instead of the roughness map.
    shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(shader, "texture3");

    // Work around a raylib 5.5 bug (rmodels.c's UploadMesh(): the
    // vertexBoneWeights "no skin data" fallback calls
    // rlSetVertexAttributeDefault(..., SHADER_ATTRIB_VEC4, 2) - count=2, but
    // that function only actually issues the GL call when count==4, so the
    // call silently no-ops). Every UNRIGGED mesh (marbles, player, the
    // mannequin) ends up with OpenGL's own fallback for a never-set
    // attribute, which is (0,0,0,1) - NOT (0,0,0,0) - so lighting.vert's "is
    // this vertex skinned at all" check fires for everything, skinning every
    // plain mesh in the scene by boneMatrices[0].
    //
    // Now that girlmodel has a REAL skin, this uniform is no longer just
    // sitting at zero forever: her draw call uploads her own 65 real bone
    // matrices into boneMatrices[0..64] via DrawMesh's
    // rlSetUniformMatrices() - and because `boneMatrices` lives on the
    // SHARED `shader` program object, that write is visible to every OTHER
    // draw call using this same shader too. So right after she's drawn,
    // boneMatrices[0] holds whatever her root bone's current animated
    // transform is, NOT identity - and the next unrigged mesh drawn (which
    // still hits that same accidental-skinning branch above) gets
    // "skinned" by her hip bone instead of left alone. That's why the
    // mannequin was rendering wrong/invisible while still casting a correct
    // shadow: the shadow pass uses pointDepthShader, a different program
    // that never reads boneMatrices at all, so it was never affected.
    //
    // Fix: explicitly stamp boneMatrices[0] back to identity immediately
    // before drawing EACH unrigged object, undoing whatever the
    // most-recently-drawn skinned object left behind. See the
    // `SetShaderValueMatrix(shader, boneMatricesLoc, MatrixIdentity())`
    // calls in the main draw loop below.
    int boneMatricesLoc = GetShaderLocation(shader, "boneMatrices");
    SetShaderValueMatrix(shader, boneMatricesLoc, MatrixIdentity());

    // Fallback normal/roughness maps for every lit material that doesn't
    // ship its own (marbles, player, the mannequin): flat blue = "no bump"
    // in tangent space, mid-gray = a medium 0.5 roughness. See
    // EnsureFallbackMaps() for why every material needs one of these.
    Texture2D flatNormalTex    = MakeSolidTexture(Color{ 128, 128, 255, 255 });
    Texture2D flatRoughnessTex = MakeSolidTexture(Color{ 128, 128, 128, 255 });

    for (auto& marble : marbles)
    {
        marble.setTerrain(&terrain);
        marble.setTiling(1.0f, 1.0f);
        marble.SetShader(shader);
        EnsureFallbackMaps(marble.Model.materials[0], flatNormalTex, flatRoughnessTex);
    }

    player.SetShader(shader);
    EnsureFallbackMaps(player.Model.materials[0], flatNormalTex, flatRoughnessTex);
    //o1.SetMaterialMapDiffuse(texture);


    terrain.lightIntensity = LIGHT_INTENSITY;
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
        EnsureFallbackMaps(importedModel.materials[i], flatNormalTex, flatRoughnessTex);
    }
    importedModelPosition = { 75.0f, terrain.getHeightAt(75.0f, 75.0f), 75.0f };

    // -----------------------------------------------------------------------
    // girlmodel: IQM doesn't embed texture images (unlike the old glTF
    // export) - raylib's IQM loader just guesses a filename from each
    // material's name and calls LoadTexture() on it, which fails here and
    // leaves that material's albedo as an invalid, unbound texture (renders
    // solid black, confirmed by the "Failed to open file" warnings printed
    // at startup). Those same warnings also reveal the mesh load order, so
    // this table maps each of the 6 meshes to its real texture files, which
    // still exist as loose PNGs from the original Tripo export. The two
    // body materials never had a separate loose normal-map file, hence
    // `nullptr` there - EnsureFallbackMaps() below fills that gap with the
    // flat "no bump" texture, same as it does for marbles/player/mannequin.
    // -----------------------------------------------------------------------
    // NOTE: every path below points at a "_patched" copy, not the original
    // Tripo export. The originals have large unpainted (pure black) gaps
    // between UV islands - not a code/shader bug, an asset defect - which a
    // one-off dilation pass (nearest-painted-pixel fill) patched over; see
    // the flat-colored "_patched" PNGs sitting next to their originals in
    // this same folder.
    struct GirlMaterialTextures { const char* albedo; const char* normal; const char* roughness; };
    constexpr GirlMaterialTextures GIRL_MATERIAL_TEXTURES[] = {
        { "models/girlmodel/textures/tripo_node_dd344864-1c26-4606-b384-8dd3fb3be0d3_BaseColor_4_patched.png",
          "models/girlmodel/textures/tripo_node_dd344864-1c26-4606-b384-8dd3fb3be0d3_Normal_Bake__patched.png",
          "models/girlmodel/textures/tripo_model_roughness_5@channels=G_patched.png" },   // 0: shorts
        { "models/girlmodel/textures/tripo_node_2611fd38-b321-43f0-9854-e1d5e939a4b6_BaseColor_1_patched.png",
          "models/girlmodel/textures/tripo_node_2611fd38-b321-43f0-9854-e1d5e939a4b6_Normal_Bake__patched.png",
          "models/girlmodel/textures/tripo_model_roughness_2@channels=G_patched.png" },   // 1: shirt
        { "models/girlmodel/textures/tripo_node_c133de81-b5bc-4a12-9843-79fd09d4836c_BaseColor_7_patched.png",
          "models/girlmodel/textures/tripo_node_c133de81-b5bc-4a12-9843-79fd09d4836c_Normal_Bake__patched.png",
          "models/girlmodel/textures/tripo_model_roughness_8@channels=G_patched.png" },   // 2: hair
        { "models/girlmodel/textures/tripo_node_9a62303e-5aa4-4a03-a1a8-ecd422263a2d_BaseColor_16_patched.png",
          "models/girlmodel/textures/tripo_node_9a62303e-5aa4-4a03-a1a8-ecd422263a2d_Normal_Bake__patched.png",
          "models/girlmodel/textures/tripo_model_roughness_17@channels=G_patched.png" },  // 3: sneakers
        { "models/girlmodel/textures/diffuse_body_10_patched.png", nullptr,
          "models/girlmodel/textures/roughness_body_11@channels=G_patched.png" },        // 4: body/head
        { "models/girlmodel/textures/diffuse_body1_13_patched.png", nullptr,
          "models/girlmodel/textures/roughness_body1_14@channels=G_patched.png" },       // 5: body/skin
    };
    constexpr int GIRL_MATERIAL_TEXTURE_COUNT = sizeof(GIRL_MATERIAL_TEXTURES) / sizeof(GIRL_MATERIAL_TEXTURES[0]);

    Model girlModel = LoadModel(GIRL_MODEL_PATH);
    for (int i = 0; i < girlModel.materialCount; i++)
    {
        girlModel.materials[i].shader = shader;

        if (i < GIRL_MATERIAL_TEXTURE_COUNT)
        {
            const GirlMaterialTextures& tex = GIRL_MATERIAL_TEXTURES[i];
            // Overwrites the broken albedo IQM already (unsuccessfully)
            // tried to set - see the comment above.
            girlModel.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = LoadTexture(tex.albedo);
            if (tex.normal != nullptr) girlModel.materials[i].maps[MATERIAL_MAP_NORMAL].texture = LoadTexture(tex.normal);
            girlModel.materials[i].maps[MATERIAL_MAP_ROUGHNESS].texture = LoadTexture(tex.roughness);
        }

        EnsureFallbackMaps(girlModel.materials[i], flatNormalTex, flatRoughnessTex);
    }
    // Placed a few units from the mannequin so both are visible without
    // overlapping; terrain.getHeightAt() drops her onto the ground there.
    girlModelPosition = { 65.0f, terrain.getHeightAt(65.0f, 65.0f), 65.0f };

    // -----------------------------------------------------------------------
    // girlmodel's animations: LoadModelAnimations() reads whatever animation
    // clips are baked into the same glTF/glb as the mesh - it's fine to call
    // this even before she's been rigged (see Mixamo/Blender workflow notes),
    // it'll just come back with animCount == 0 and everything below becomes
    // a no-op, drawing her in the bind pose exactly like before.
    // -----------------------------------------------------------------------
    int girlAnimCount = 0;
    ModelAnimation* girlAnimations = LoadModelAnimations(GIRL_MODEL_PATH, &girlAnimCount);
    float girlAnimTime = 0.0f; // seconds of playback into the CURRENT clip; reset to 0 on every switch
    int girlCurrentAnim = 0;   // index into girlAnimations - which clip is playing right now

    // Each clip is named after whatever Blender Action it came from (Mixamo
    // clip name if you didn't rename it) - print them once so you can see
    // what's actually available without cracking open Blender again.
    for (int i = 0; i < girlAnimCount; i++)
    {
        TraceLog(LOG_INFO, "girlmodel animation %d: \"%s\" (%d frames)", i, girlAnimations[i].name, girlAnimations[i].frameCount);
    }

    std::vector<Projectile> projectiles;

    DisableCursor();
    SetTargetFPS(120);

    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    RenderTexture2D shadowCubemap = LoadShadowCubemapRenderTexture(SHADOWMAP_RESOLUTION);

    SetShaderValue(shader, GetShaderLocation(shader, "farPlane"), &farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(terrainShader, GetShaderLocation(terrainShader, "farPlane"), &farPlane, SHADER_UNIFORM_FLOAT);

    // SetShaderValueTexture() assumes GL_TEXTURE_2D and can't bind a cubemap
    // (a GL texture's target is fixed at first bind), so bind the depth
    // cubemap to a fixed texture unit manually and point the sampler at it.
    constexpr int SHADOW_CUBEMAP_TEXTURE_UNIT = 1;
    rlActiveTextureSlot(SHADOW_CUBEMAP_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_CUBE_MAP, shadowCubemap.depth.id);
    rlActiveTextureSlot(0);

    int shadowUnit = SHADOW_CUBEMAP_TEXTURE_UNIT;
    SetShaderValue(shader, GetShaderLocation(shader, "shadowCubemap"), &shadowUnit, SHADER_UNIFORM_INT);
    SetShaderValue(terrainShader, GetShaderLocation(terrainShader, "shadowCubemap"), &shadowUnit, SHADER_UNIFORM_INT);

    while (!WindowShouldClose()) {
        // --- Update -----------------------------------------------------
        auto now = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        // FPV camera: driven by the player's position/look direction instead
        // of raylib's free camera.
        player.HandleInput(deltaTime);
        player.update(deltaTime);

        camera.position = player.GetEyePosition();
        camera.target   = Vector3Add(camera.position, player.GetForward());

        for (auto& marble : marbles)
        {
            marble.update(deltaTime);
        }

        // Manual test harness: number keys 1-9 jump straight to that clip
        // index, if girlmodel has that many. An instant cut - see
        // PlayGirlAnimation()'s comment for why there's no blending yet.
        for (int key = KEY_ONE; key <= KEY_NINE; key++)
        {
            if (IsKeyPressed(key))
            {
                PlayGirlAnimation(key - KEY_ONE, girlAnimCount, girlCurrentAnim, girlAnimTime);
            }
        }

        // Advance girlmodel's animation and re-pose her skeleton. This is
        // GPU skinning: UpdateModelAnimationBones() only recomputes
        // model.meshes[i].boneMatrices on the CPU (cheap) - the actual
        // per-vertex blending happens in lighting.vert on the GPU every
        // draw call. UpdateModelAnimation() (no "Bones" suffix) would
        // instead re-skin every vertex on the CPU AND re-upload the vertex
        // buffer, which would fight with what the shader is already doing -
        // don't call both.
        if (girlAnimCount > 0)
        {
            ModelAnimation& girlAnim = girlAnimations[girlCurrentAnim];
            if (IsModelAnimationValid(girlModel, girlAnim))
            {
                girlAnimTime += deltaTime;
                int frame = (int)(girlAnimTime * GIRL_ANIM_FPS) % girlAnim.frameCount;
                UpdateModelAnimationBones(girlModel, girlAnim, frame);
            }
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

            DrawSceneForShadows(importedModel, girlModel, marbles, terrain, pointDepthShader);
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
            SetShaderValue(shader, lightPosLoc, &lightPos, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, viewPosLoc, &camera.position, SHADER_UNIFORM_VEC3);

            

            DrawXYZLines();
            DrawGrid(10, 1.0f);

            // Undoes whatever the LAST frame's girlModel draw left in the
            // shared `boneMatrices[0]` uniform slot (see the long comment
            // where boneMatricesLoc is set, above) - without this, the
            // mannequin would be "skinned" by her hip bone instead of
            // rendering as a plain, un-skinned mesh.
            SetShaderValueMatrix(shader, boneMatricesLoc, MatrixIdentity());
            DrawModel(importedModel, importedModelPosition, 1.0f, WHITE);

            // Backface culling off: girlmodel's hair/clothing meshes are
            // single-layer shells authored `doubleSided` in the glTF, so
            // with culling on, whichever side normally faces away from the
            // camera would just vanish depending on view angle.
            rlDisableBackfaceCulling();
            DrawModel(girlModel, girlModelPosition, GIRL_MODEL_SCALE, WHITE);
            rlEnableBackfaceCulling();

            // Same reset again: girlModel's draw just above just wrote her
            // OWN real bone matrices into the shared slot, so anything
            // unrigged drawn after her (marbles, if any exist) needs this
            // undone too, this time within the SAME frame.
            SetShaderValueMatrix(shader, boneMatricesLoc, MatrixIdentity());

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

    //UnloadTexture(marbleTexture);
    if (girlAnimCount > 0) UnloadModelAnimations(girlAnimations, girlAnimCount);
    UnloadModel(importedModel);
    UnloadModel(girlModel);
    UnloadTexture(flatNormalTex);
    UnloadTexture(flatRoughnessTex);
    CloseWindow();

    return 0;
}

// Load render texture for shadowmap projection
// NOTE: Load framebuffer with only a texture depth attachment,
// no color attachment required for shadowmap

