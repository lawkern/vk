#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec3 out_uv;

struct vertex
{
   vec3 position;
   float uv_x;

   vec3 normal;
   float uv_y;

   vec4 color;
};

layout(buffer_reference, std430) readonly buffer vertex_buffer
{
   vertex vertices[];
};

layout(push_constant) uniform constants {
   mat4 render_matrix;
   vertex_buffer vertex_buffer;
} push_constants;

void main(void)
{
   vertex v = push_constants.vertex_buffer.vertices[gl_VertexIndex];

   gl_Position = push_constants.render_matrix * vec4(v.position, 1);
   out_color = v.color.xyz;
   out_uv.x = v.uv_x;
   out_uv.y = v.uv_y;
}
