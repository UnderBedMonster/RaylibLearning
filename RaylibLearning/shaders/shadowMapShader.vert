#version 330

in vec3 vertexPosition;

// Skinning inputs - see lighting.vert for the full explanation. Without
// these, this depth pass would render every skinned mesh (girlmodel) in its
// bind (T-pose) position while the color pass renders her current ANIMATED
// pose - the shadow cubemap would then hold depth values for a completely
// different pose than the one actually being lit, causing the animated
// surface to self-shadow at effectively random per-fragment noise (since an
// animated point's true depth from the light has no consistent relationship
// to its bind-pose depth). That mismatch, not bias, was the real source of
// the fine speckled "shadow acne" seen across her skin.
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;

uniform mat4 mvp;    // model * view * projection (raylib's default naming)
uniform mat4 matModel;

#define MAX_BONE_NUM 128
uniform mat4 boneMatrices[MAX_BONE_NUM];

out vec3 fragPos;

void main()
{
    vec4 localPosition = vec4(vertexPosition, 1.0);

    float totalWeight = vertexBoneWeights.x + vertexBoneWeights.y + vertexBoneWeights.z + vertexBoneWeights.w;
    if (totalWeight > 0.0001)
    {
        mat4 skinMatrix =
            vertexBoneWeights.x * boneMatrices[int(vertexBoneIds.x)] +
            vertexBoneWeights.y * boneMatrices[int(vertexBoneIds.y)] +
            vertexBoneWeights.z * boneMatrices[int(vertexBoneIds.z)] +
            vertexBoneWeights.w * boneMatrices[int(vertexBoneIds.w)];

        localPosition = skinMatrix * localPosition;
    }

    fragPos = vec3(matModel * localPosition); // world-space position
    gl_Position = mvp * localPosition;          // clip-space position for this pass
}
