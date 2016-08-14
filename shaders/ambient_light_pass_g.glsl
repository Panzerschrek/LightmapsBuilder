#version 430

layout( triangles, invocations= 1 ) in;
layout( triangle_strip, max_vertices = 3 ) out;

flat in float g_lightmap_layer[];

void main()
{
	gl_Layer= int(g_lightmap_layer[0]);

	gl_Position= gl_in[0].gl_Position;
	EmitVertex();
	gl_Position= gl_in[1].gl_Position;
	EmitVertex();
	gl_Position= gl_in[2].gl_Position;
	EmitVertex();

	EndPrimitive();
}