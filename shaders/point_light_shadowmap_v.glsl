#version 430

in vec3 pos;
in vec2 tex_coord;
in vec2 lightmap_coord;
in uint lightmap_layer;
in vec3 normal;

out vec3 g_pos;
out vec3 g_lightmap_coord;

void main()
{
	g_pos= pos;
	g_lightmap_coord= vec3( lightmap_coord, float(lightmap_layer) + 0.01 );
	gl_Position= vec4( pos, 1.0 );
}