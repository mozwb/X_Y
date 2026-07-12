#type vertex
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

out vec3 v_Normal;
out vec3 v_Position;

void main()
{
    vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
    gl_Position = u_ViewProjection * worldPos;
    v_Normal   = normalize(mat3(u_Transform) * a_Normal);
    v_Position = worldPos.xyz;
}

#type fragment
#version 330 core
in vec3 v_Normal;
in vec3 v_Position;

out vec4 FragColor;

void main()
{
    // 用法线作为颜色调试：normal 范围 [-1,1] → 颜色 [0,1]
    vec3 debugColor = normalize(v_Normal) * 0.5 + 0.5;
    FragColor = vec4(debugColor, 1.0);
}