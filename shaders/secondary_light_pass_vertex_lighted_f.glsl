uniform sampler2DArray textures[8];

in vec3 f_light;
in float f_texture_array;
in vec3 f_tex_coord;

out vec4 color;

void main()
{
	vec4 c= texture( textures[int(f_texture_array)], f_tex_coord );

#ifdef ALPHA_TEST
	if( c.a < 0.5 )
		discard;
#endif

	color= vec4( c.xyz * f_light, 1.0 );
}
