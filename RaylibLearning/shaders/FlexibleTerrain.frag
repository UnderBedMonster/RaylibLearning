#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in float fragHeight;
in vec3 fragPos;
in float fragDeform;
in vec4 fragColor;  // Raylib passes vertex color here automatically

uniform mat4  matModel;    // Raylib fills this automatically

uniform float minHeight;  
uniform float maxHeight; 
uniform vec3 lightPos;
uniform float lightIntensity;

out vec4 finalColor;


void main() {
    float deformT = fragColor.r;  // 0.0 = no deform, 1.0 = max deform

    vec3 green  = vec3(17.0/255.0,  255.0/255.0, 0.0);
    vec3 yellow = vec3(251.0/255.0, 255.0/255.0, 0.0);
    vec3 red    = vec3(1.0, 0.0, 0.0);
    vec3 black  = vec3(0.0, 0.0, 0.0);

    vec3 color;
    if      (deformT < 0.33) color = mix(green,  yellow, deformT / 0.33);
    else if (deformT < 0.66) color = mix(yellow, red,    (deformT - 0.33) / 0.33);
    else                     color = mix(red,    black,  (deformT - 0.66) / 0.34);

    finalColor = vec4(color, 1.0);
}
 