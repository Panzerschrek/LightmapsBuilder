layout( triangles, invocations= 1 ) in;
layout( triangle_strip, max_vertices = 3 ) out;

in vec3 g_pos[];
in vec3 g_normal[];
flat in float g_lightmap_layer[];

out vec3 f_pos;
out vec3 f_normal;

void main()
{
	gl_Layer= int(g_lightmap_layer[0]);

	gl_Position= gl_in[0].gl_Position;
	f_pos= g_pos[0];
	f_normal= g_normal[0];
	EmitVertex();

	gl_Position= gl_in[1].gl_Position;
	f_pos= g_pos[1];
	f_normal= g_normal[1];
	EmitVertex();

	gl_Position= gl_in[2].gl_Position;
	f_pos= g_pos[2];
	f_normal= g_normal[2];
	EmitVertex();

	EndPrimitive();
}
