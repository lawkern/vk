#version 450

layout(location = 0) out vec3 out_color;

void main(void)
{
   const vec3 positions[3] = vec3[3](vec3(0.5, 0.5, 0), vec3(-0.5, 0.5, 0), vec3(0, -0.5, 0));
   const vec3 colors[3] = vec3[3](vec3(0.5, 0, 0), vec3(0, 0.5, 0), vec3(0, 0, 0.5));

   gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
   out_color = colors[gl_VertexIndex];
}
