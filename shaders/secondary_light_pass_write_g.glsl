layout( points, invocations= 1 ) in;
layout( points, max_vertices = 1 ) out;

in vec3 g_coord[];

void main()
{
	gl_Layer= int(g_coord[0].z);

	gl_Position= vec4( g_coord[0].xy, 1.0, 1.0 );

	EmitVertex();
	EndPrimitive();
}
