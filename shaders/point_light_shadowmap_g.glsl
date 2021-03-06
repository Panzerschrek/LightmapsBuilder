layout( triangles, invocations= 6 ) in;
layout( triangle_strip, max_vertices = 3 ) out;

uniform mat4 view_matrices[6];
uniform int cubemap_side_count= 6;

in vec3 g_pos[];
in vec3 g_lightmap_coord[];
in vec3 g_tex_coord[];
in float g_texture_array[];

out vec4 f_pos;
out vec3 f_lightmap_coord;
out vec3 f_tex_coord;
out float f_texture_array;

void main()
{
	int i= gl_InvocationID;

	gl_Layer= i;
	gl_Position= f_pos= view_matrices[i] * gl_in[0].gl_Position;
	f_lightmap_coord= g_lightmap_coord[0];
	f_tex_coord= g_tex_coord[0];
	f_texture_array= g_texture_array[0];
	EmitVertex();

	gl_Position= f_pos= view_matrices[i] * gl_in[1].gl_Position;
	f_lightmap_coord= g_lightmap_coord[1];
	f_tex_coord= g_tex_coord[1];
	f_texture_array= g_texture_array[1];
	EmitVertex();

	gl_Position= f_pos= view_matrices[i] * gl_in[2].gl_Position;
	f_lightmap_coord= g_lightmap_coord[2];
	f_tex_coord= g_tex_coord[2];
	f_texture_array= g_texture_array[2];
	EmitVertex();

	EndPrimitive();
}
