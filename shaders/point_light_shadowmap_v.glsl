in vec3 pos;
in vec2 tex_coord;
in vec2 lightmap_coord;
in ivec3 tex_maps;
in vec3 normal;

out vec3 g_pos;
out vec3 g_lightmap_coord;

void main()
{
	g_pos= pos;
	g_lightmap_coord= vec3( lightmap_coord, float(tex_maps.z) + 0.01 );
	gl_Position= vec4( pos, 1.0 );
}
