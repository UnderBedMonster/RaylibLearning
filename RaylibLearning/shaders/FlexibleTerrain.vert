#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;  


uniform mat4 mvp;
uniform mat4 matModel;
uniform vec3 terrainOffset;

out vec3 worldZeroPoint;
out vec2 fragTexCoord;
out vec3 fragNormal;
out float fragHeight;
out vec3 fragPos;
out vec4 fragColor;

void main()
{
    fragColor = vertexColor;
    fragTexCoord = vertexTexCoord;
    fragNormal   = vertexNormal;
    fragHeight   = vertexPosition.y - terrainOffset.y;   
    vec4 worldPos = matModel * vec4(vertexPosition, 1.0);
    fragPos       = worldPos.xyz;
    gl_Position = mvp * vec4(vertexPosition,1.0);
}