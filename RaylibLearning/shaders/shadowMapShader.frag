#version 330


// point_depth.fs
uniform vec3 lightPos;
uniform float farPlane;   // add this
in vec3 fragPos; // world space position, passed from vertex shader

void main()
{
    float lightDistance = length(fragPos - lightPos);
    lightDistance = lightDistance / farPlane; // normalize to [0,1]
    gl_FragDepth = lightDistance;
}