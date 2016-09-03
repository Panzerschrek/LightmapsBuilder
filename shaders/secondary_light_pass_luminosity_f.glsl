uniform sampler2DArray lightmap;
uniform sampler2DArray textures[8];

in vec3 f_tex_coord;
in float f_texture_array;
in vec3 f_lightmap_coord;

out vec4 color;

void main()
{
#ifdef AVERAGE_LIGHT
	const float max_lod= 16.0;
	vec3 c= textureLod( textures[int(f_texture_array)], f_tex_coord, max_lod ).xyz;
#else
	vec3 c= texture( textures[int(f_texture_array)], f_tex_coord ).xyz;
#endif

	vec3 light= c * f_lightmap_coord.x;

	color= vec4( light, 1.0 );
}
