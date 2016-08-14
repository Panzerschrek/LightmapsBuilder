#version 400

in vec3 pos;
in vec2 tex_coord;
in vec2 lightmap_coord;
in uint lightmap_layer;
in vec3 normal;

out vec3 g_pos;
out vec3 g_normal;
flat out float g_lightmap_layer;

void main()
{
	g_pos= pos;
	g_normal= normal;
	g_lightmap_layer= float(lightmap_layer) + 0.01;
	gl_Position= vec4( lightmap_coord * vec2(2.0, 2.0) - vec2(1.0,1.0), 0.0, 1.0 );
}