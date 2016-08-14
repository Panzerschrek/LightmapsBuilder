uniform vec4 clip_plane;

in vec3 pos;
in vec2 tex_coord;
in vec2 lightmap_coord;
in vec3 tex_maps;
in vec3 normal;

out vec3 g_tex_coord;
out float g_texture_array;
out vec3 g_lightmap_coord;

const float eps= 0.01f;

void main()
{
	g_tex_coord= vec3( tex_coord, float(tex_maps.y) + eps );
	g_texture_array= float(tex_maps.x) + eps;

	g_lightmap_coord= vec3( lightmap_coord, float(tex_maps.z) + eps );

	vec4 pos4= vec4( pos, 1.0 );
	gl_ClipDistance[0]= dot(pos4, clip_plane );
	gl_Position= pos4;
}
