uniform float inv_max_light_dst;

uniform sampler2DArray textures[8];

out vec4 color;

in vec4 f_pos;
in vec3 f_lightmap_coord;
in vec3 f_tex_coord;
in float f_texture_array;

void main()
{
#ifdef ALPHA_TEST
	float a= texture( textures[int(f_texture_array)], f_tex_coord ).a;
	if( a < 0.5 )
		discard;
#endif

	float l= length(f_pos.xyw);
	gl_FragDepth = min( inv_max_light_dst * l, 1.0 );
	color= vec4 ( l, l, l, 1.0 );
}
