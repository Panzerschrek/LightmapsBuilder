uniform samplerCube cubemap;
uniform samplerCube cubemap_multiplier;
uniform sampler2D tex;

in vec2 f_pos;
in vec2 f_tex_coord;

out vec4 color;

const float pi= 3.1415926535;

void main()
{
	/*
	vec3 coord;

	float a= f_tex_coord.x * pi * 2.0;
	float b= f_tex_coord.y * pi - pi * 0.5;
	coord.y= sin(-b);
	coord.x= coord.z= cos(b);
	coord.x*= sin(a);
	coord.z*= cos(a);

	vec3 tex_data= texture( cubemap, coord ).xyz;
	float mult= 0.1 * texture( cubemap_multiplier, -coord ).x;
	color= vec4( tex_data * mult, 1.0 );
	*/
	color= 0.1 * texture( tex, f_tex_coord );
}
