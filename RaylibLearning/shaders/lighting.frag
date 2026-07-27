#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// texture0/1/2 are auto-wired by raylib to whichever texture is sitting in
// Material.maps[MATERIAL_MAP_ALBEDO/METALNESS/NORMAL] (see LoadShaderFromMemory
// in rcore.c). texture3 (roughness, MATERIAL_MAP_ROUGHNESS) is NOT auto-wired -
// raylib only knows the names "texture0/1/2" out of the box, so main.cpp has
// to manually point shader.locs[SHADER_LOC_MAP_ROUGHNESS] at "texture3" once,
// right after LoadShader(), or this sampler would silently read texture unit 0
// (the albedo) instead of unit 3.
uniform sampler2D texture0; // albedo / base color (RGB)
uniform sampler2D texture2; // tangent-space normal map (RGB, packed 0..1 -> -1..1)
uniform sampler2D texture3; // roughness map - a standalone single-channel grayscale image; raylib
                             // uploads GRAYSCALE images as GL_R8/GL_RED with no channel swizzle, so
                             // the real value lands in .r, NOT .g (the glTF *packed* metallic-roughness
                             // texture used the green channel, but these are separate per-map PNGs)

uniform vec4 colDiffuse;
uniform vec2 tiling;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform float lightIntensity;

uniform samplerCube shadowCubemap;
uniform float farPlane;

out vec4 finalColor;

float ShadowCalculation(vec3 fragPos, vec3 normal, vec3 lightDir)
{
    vec3 fragToLight = fragPos - lightPos;
    float closestDepth = texture(shadowCubemap, fragToLight).r * farPlane;
    float currentDepth = length(fragToLight);

    // Slope-scaled bias: grazing-angle surfaces need a much bigger bias to
    // avoid self-shadowing acne than surfaces facing the light head-on.
    float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
    float bias = max(1.0 * (1.0 - cosTheta), 0.05);

    return currentDepth - bias > closestDepth ? 1.0 : 0.0; // 1.0 = in shadow
}

// Builds a tangent/bitangent/normal (TBN) basis on the fly, purely from how
// world position and UV change between neighboring screen pixels (dFdx/dFdy).
// Normal maps need SOME tangent-space basis to rotate their (x,y,z) into world
// space, and the usual way to get one is a vertexTangent attribute baked into
// the mesh - but girlmodel's glTF only exports POSITION/NORMAL/TEXCOORD, no
// TANGENT. This screen-space trick sidesteps that requirement entirely, at the
// cost of being a per-pixel derivative (a little more GPU work, and it can
// look slightly noisy on very low-poly/triangle-sliver geometry - not an
// issue here).
// Source: Christian Schuler, "Normal Mapping Without Precomputed Tangents"
// (http://www.thetenthplanet.de/archives/1180)
mat3 CotangentFrame(vec3 N, vec3 worldPos, vec2 uv)
{
    // How world position shifts when moving one pixel right (dFdx) or down
    // (dFdy) on screen, and how the UV coordinate shifts over that same step.
    vec3 dPosX = dFdx(worldPos);
    vec3 dPosY = dFdy(worldPos);
    vec2 dUvX  = dFdx(uv);
    vec2 dUvY  = dFdy(uv);

    // These two combinations of position-derivatives are what's left of
    // dPosX/dPosY once you project out the component already explained by
    // the surface normal N - i.e. the "in-surface" part of each derivative.
    vec3 perpDPosY = cross(dPosY, N);
    vec3 perpDPosX = cross(N, dPosX);

    // Solve for the tangent (points along increasing U) and bitangent
    // (points along increasing V) that would produce the observed UV
    // change given the observed position change.
    vec3 T = perpDPosY * dUvX.x + perpDPosX * dUvY.x;
    vec3 B = perpDPosY * dUvX.y + perpDPosX * dUvY.y;

    // Rescale T and B to unit length using one shared factor (cheaper than
    // normalizing each separately, and keeps them proportional to each other).
    float invMax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invMax, B * invMax, N);
}

// Samples the normal map and rotates it out of tangent space (where "straight
// out of the surface" is always (0,0,1)) into world space, using the basis
// built above.
vec3 ApplyNormalMap(vec3 geometricNormal, vec3 worldPos, vec2 uv)
{
    // Normal maps store the XYZ direction as an RGB color in 0..1; unpack
    // back to a -1..1 vector before using it as a direction.
    vec3 tangentSpaceNormal = texture(texture2, uv).rgb * 2.0 - 1.0;

    mat3 TBN = CotangentFrame(geometricNormal, worldPos, uv);
    return normalize(TBN * tangentSpaceNormal);
}

void main()
{
    // Diffuse texture, tiled the same way lighting.frag's predecessor did
    // (tiling stays 1,1 for the marbles/model; terrain uses its own shader).
    vec2 texCoord = fragTexCoord * tiling;
    vec4 texelColor = texture(texture0, texCoord);

    // "Geometric" normal = the coarse, per-vertex normal, before any bump
    // detail from the normal map is layered on top of it.
    vec3 geometricNormal = normalize(fragNormal);
    vec3 normal = ApplyNormalMap(geometricNormal, fragPosition, texCoord);

    vec3 lightDir = normalize(lightPos - fragPosition);
    vec3 viewDir  = normalize(viewPos - fragPosition);
    vec3 halfDir  = normalize(lightDir + viewDir); // Blinn-Phong half-vector

    // Same inverse-square-ish falloff as terrain.frag, so both shaders dim
    // consistently with distance from the one shared light.
    float distance    = length(lightPos - fragPosition);
    float attenuation = 1.0 / (1.0 + 0.01 * distance + 0.001 * distance * distance);

    // Roughness (0 = mirror-smooth, 1 = fully matte) reshapes the specular
    // highlight: rough surfaces (cloth, hair) scatter light into a wide, dim
    // highlight, while smooth ones (skin, plastic-y sneaker soles) put it
    // into a small, bright one. Objects with no real roughness map (marbles,
    // player, the mannequin) fall back to a flat 0.5 texture set up in
    // main.cpp, which lands them roughly in the middle of this range.
    float roughness     = texture(texture3, texCoord).r;
    float shininess     = mix(128.0, 4.0, roughness);
    float specularBoost = mix(0.6, 0.05, roughness);

    float diffuse  = max(dot(normal, lightDir), 0.0);
    float specular = pow(max(dot(normal, halfDir), 0.0), shininess) * specularBoost;
    float ambient  = 0.15; // floor so unlit faces aren't pure black

    float shadow = ShadowCalculation(fragPosition, normal, lightDir);
    // NOTE: the previous version of this shader computed `specular` above but
    // never actually added it in here - it was dead code, so nothing ever
    // showed a highlight. Now that roughness drives its shape, it's folded
    // into the final light term so it actually renders.
    float light = ambient + (1.0 - shadow) * (diffuse + specular) * attenuation * lightIntensity;

    finalColor = texelColor * fragColor * colDiffuse * vec4(vec3(light), 1.0);
}
