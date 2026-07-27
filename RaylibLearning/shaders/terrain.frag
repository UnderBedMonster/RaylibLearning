#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in float fragHeight;
in vec3 fragPos;

uniform mat4  matModel;    // Raylib fills this automatically

uniform float minHeight;  
uniform float maxHeight; 
uniform vec3 lightPos;
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

void main()
{

    float lightDistance = length(fragPos - lightPos);
    lightDistance = lightDistance / farPlane;

    float t = (fragHeight - minHeight) / (maxHeight - minHeight);
    t = clamp(t, 0.0, 1.0);

    vec3 dx = dFdx(fragPos);
    vec3 dy = dFdy(fragPos);
    vec3 flatNormal = normalize(cross(dx, dy));

    vec3 deepWater  = vec3(0.0, 0.1, 0.6);
    vec3 sand       = vec3(0.8, 0.7, 0.4);
    vec3 grass      = vec3(0.1, 0.5, 0.1);
    vec3 rock       = vec3(0.4, 0.35, 0.3);
    vec3 snow       = vec3(1.0, 1.0, 1.0);

    vec3 color;
    if      (t < 0.2) color = mix(deepWater, sand,  t /0.2);
    else if (t < 0.4) color = mix(sand,      grass, (t - 0.2) / 0.2);
    else if (t < 0.7) color = mix(grass,     rock,  (t - 0.4) /0.3);
    else               color = mix(rock,      snow,  (t - 0.7) /0.3);

    vec3  lightDir  = normalize(lightPos - fragPos);   // direction FROM fragment TO light
    float distance  = length(lightPos - fragPos); 
    
    float attenuation = 1.0 / (1.0 + 0.01 * distance + 0.001 * distance * distance);

    float diffuse  = max(dot(flatNormal, lightDir), 0.0);
    float ambient  = 0.15;
    float shadow = ShadowCalculation(fragPos, flatNormal, lightDir);
    float light = ambient + (1.0 - shadow) * diffuse * attenuation * lightIntensity;


    finalColor = vec4(color * light,1.0);
}


