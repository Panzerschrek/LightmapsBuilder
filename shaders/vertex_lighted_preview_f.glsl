uniform float brightness;
uniform float gray_factor;

uniform sampler2DArray textures[8];

in vec3 f_pos;
in vec3 f_tex_coord;
in vec3 f_light;
in vec3 f_normal;
in float f_texture_array;

out vec4 color;

void main()
{
	vec4 c= texture( textures[int(f_texture_array)], f_tex_coord );
#ifdef ALPHA_TEST
	if( c.a < 0.5 )
		discard;
#endif

	c.xyz= mix( c.xyz, vec3( 0.6, 0.6, 0.6 ), gray_factor );
	vec3 linear_color= brightness * c.xyz * f_light;

	color= vec4( vec3(1.0, 1.0, 1.0) - exp(-0.1 * linear_color), 1.0);
	//color= vec4( linear_color, 1.0 );
}
