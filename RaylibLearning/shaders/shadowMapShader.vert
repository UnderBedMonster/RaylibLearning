#version 330

uniform samplerCube shadowCubemap;
uniform vec3 lightPos;
uniform float farPlane;

float ShadowCalculation(vec3 fragPos)
{
    vec3 fragToLight = fragPos - lightPos;
    float closestDepth = texture(shadowCubemap, fragToLight).r * farPlane;
    float currentDepth = length(fragToLight);
    float bias = 0.05;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0; // 1.0 = in shadow
}
