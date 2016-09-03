uniform sampler2DArray lightmap;
uniform sampler2DArray textures[8];

in vec3 f_tex_coord;
in float f_texture_array;
in vec3 f_lightmap_coord;

out vec4 color;

void main()
{
	vec4 c= texture( textures[int(f_texture_array)], f_tex_coord );

#ifdef ALPHA_TEST
	if( c.a < 0.5 )
		discard;
#endif

	vec3 lightmap_light= texture( lightmap, f_lightmap_coord ).xyz;

	color= vec4( c.xyz * lightmap_light, 1.0 );
}
