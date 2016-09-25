layout( triangles, invocations= 6 ) in;
layout( triangle_strip, max_vertices = 3 ) out;

uniform mat4 view_matrices[6];

in vec3 g_tex_coord[];
in float g_texture_array[];
in vec3 g_light[];

out vec3 f_tex_coord;
out float f_texture_array;
out vec3 f_light;

void main()
{
	int i= gl_InvocationID;
	gl_Layer= i;

	gl_Position= view_matrices[i] * gl_in[0].gl_Position;
	gl_ClipDistance[0]= gl_in[0].gl_ClipDistance[0];
	f_tex_coord= g_tex_coord[0];
	f_texture_array= g_texture_array[0];
	f_light= g_light[0];
	EmitVertex();

	gl_Position= view_matrices[i] * gl_in[1].gl_Position;
	gl_ClipDistance[0]= gl_in[1].gl_ClipDistance[0];
	f_tex_coord= g_tex_coord[1];
	f_texture_array= g_texture_array[1];
	f_light= g_light[1];
	EmitVertex();

	gl_Position= view_matrices[i] * gl_in[2].gl_Position;
	gl_ClipDistance[0]= gl_in[2].gl_ClipDistance[0];
	f_tex_coord= g_tex_coord[2];
	f_texture_array= g_texture_array[2];
	f_light= g_light[2];
	EmitVertex();

	EndPrimitive();
}
