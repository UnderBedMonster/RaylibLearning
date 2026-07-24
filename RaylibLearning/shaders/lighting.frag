#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 tiling;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform float lightIntensity;

out vec4 finalColor;

void main()
{
    // Diffuse texture, tiled the same way lighting.frag's predecessor did
    // (tiling stays 1,1 for the marbles/model; terrain uses its own shader).
    vec2 texCoord = fragTexCoord * tiling;
    vec4 texelColor = texture(texture0, texCoord);

    vec3 normal   = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPosition);
    vec3 viewDir  = normalize(viewPos - fragPosition);
    vec3 halfDir  = normalize(lightDir + viewDir); // Blinn-Phong half-vector

    // Same inverse-square-ish falloff as terrain.frag, so both shaders dim
    // consistently with distance from the one shared light.
    float distance    = length(lightPos - fragPosition);
    float attenuation = 1.0 / (1.0 + 0.01 * distance + 0.001 * distance * distance);

    float diffuse  = max(dot(normal, lightDir), 0.0);
    float specular = pow(max(dot(normal, halfDir), 0.0), 32.0) * 0.3; // narrow, dim highlight
    float ambient  = 0.15; // floor so unlit faces aren't pure black

    float light = ambient + (diffuse + specular) * attenuation * lightIntensity;

    finalColor = texelColor * fragColor * colDiffuse * vec4(vec3(light), 1.0);
}
