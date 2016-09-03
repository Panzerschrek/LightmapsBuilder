in vec3 f_tex_coord;
in float f_texture_array;

uniform sampler2DArray textures[8];

void main()
{
#ifdef ALPHA_TEST
	float a= texture( textures[int(f_texture_array)], f_tex_coord ).a;
	if( a < 0.5 )
		discard;
#endif
}
