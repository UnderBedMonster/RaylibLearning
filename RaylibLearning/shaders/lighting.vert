#version 330

// These attribute names are the raylib defaults (see rcore.c's
// LoadShaderFromMemory) - any mesh loaded through raylib (LoadModel,
// GenMeshSphere, GenMeshCube, ...) already uploads its data under exactly
// these names, so nothing extra needs wiring up on the C++ side for them.
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Skinning inputs - only meaningful for a mesh that was exported with an
// armature (a glTF "skin": JOINTS_0/WEIGHTS_0 per vertex). For a mesh with
// no armature (marbles, player, the mannequin, and girlmodel BEFORE she's
// rigged), raylib disables these two attributes at the GPU level and feeds
// every vertex the constant value (0,0,0,0) instead - see rmodels.c's
// UploadMesh(), the `else` branches around RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS/BONEWEIGHTS.
// That's why main() below treats all-zero weights as "not skinned" rather
// than assuming bone 0's transform applies.
in vec4 vertexBoneIds;     // up to 4 bone indices influencing this vertex (arrives as plain floats like 0.0, 1.0, 2.0 - NOT normalized, despite being stored as bytes)
in vec4 vertexBoneWeights; // how much each of those 4 bones influences this vertex; the 4 weights sum to ~1.0 for a skinned vertex

// mvp/matModel/matNormal are never set from C++: raylib's DrawModel/DrawMesh
// auto-detects uniforms with these exact names and fills them in every call.
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal; // = transpose(inverse(matModel)), so normals stay perpendicular to the surface even after non-uniform scaling

// Current pose of every bone in the skeleton, uploaded once per draw call by
// raylib (see DrawMesh in rmodels.c: `rlSetUniformMatrices(locs[SHADER_LOC_BONE_MATRICES], mesh.boneMatrices, mesh.boneCount)`),
// itself computed on the CPU each frame by UpdateModelAnimationBones().
// MAX_BONE_NUM just sizes the array; it only needs to be >= the rigged
// model's actual bone count (check model.boneCount after LoadModel()) - bump
// it if a bigger skeleton ever needs more room.
#define MAX_BONE_NUM 128
uniform mat4 boneMatrices[MAX_BONE_NUM];

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

void main()
{
    // Blend up to 4 bones' current transforms together, weighted by how
    // much each influences this vertex, to get the vertex's final "skin"
    // matrix. Skipped entirely (falls back to the vertex's own, un-skinned
    // position) when every weight is 0 - i.e. for meshes with no armature -
    // since blending in boneMatrices[0] with weight 0 would still be wrong
    // (that slot holds a real bone's matrix once ANY skinned model is drawn
    // this frame) and blending with weight 1 would be worse (moves the mesh
    // to wherever that unrelated bone happens to be).
    vec4 localPosition = vec4(vertexPosition, 1.0);
    vec3 localNormal = vertexNormal;

    float totalWeight = vertexBoneWeights.x + vertexBoneWeights.y + vertexBoneWeights.z + vertexBoneWeights.w;
    if (totalWeight > 0.0001)
    {
        mat4 skinMatrix =
            vertexBoneWeights.x * boneMatrices[int(vertexBoneIds.x)] +
            vertexBoneWeights.y * boneMatrices[int(vertexBoneIds.y)] +
            vertexBoneWeights.z * boneMatrices[int(vertexBoneIds.z)] +
            vertexBoneWeights.w * boneMatrices[int(vertexBoneIds.w)];

        localPosition = skinMatrix * localPosition;
        localNormal = mat3(skinMatrix) * localNormal;
    }

    // World-space position/normal, NOT clip-space - the fragment shader
    // needs these (for lighting math and for the shadow cubemap lookup),
    // while gl_Position below needs the clip-space one instead.
    fragPosition = vec3(matModel * localPosition);
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(mat3(matNormal) * localNormal);

    // NOTE: there's no vertexTangent attribute here, even though
    // lighting.frag does normal-mapping (which usually needs a tangent
    // basis). girlmodel's glTF mesh doesn't export tangents, so instead of
    // requiring them, the fragment shader derives a tangent basis per-pixel
    // from screen-space derivatives - see CotangentFrame() in lighting.frag.

    gl_Position = mvp * localPosition;
}
