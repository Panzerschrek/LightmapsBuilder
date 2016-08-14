#version 430

uniform samplerCube cubemap;
uniform samplerCube cubemap_multipler;

in vec2 f_pos;
in vec2 f_tex_coord;

out vec4 color;

const float pi= 3.1415926535;

void main()
{
	vec3 coord;

	/*if( f_pos.x < 0.25 )
		coord= vec3( f_pos.x * 8.0 - 1.0 , f_pos.y * 8.0 - 5.0, -1.0 );
	else if( f_pos.x < 0.5 )
	{
		if( f_pos.y < 0.5 )
			coord= vec3( 0.0, 1.0, 0.0 );
		else if( f_pos.y < 0.75 )
			coord= vec3( 1.0, f_pos.y * 8.0 - 3.0, -f_pos.x * 8.0 + 3.0 );
		else
			coord= vec3( 0.0, -1.0, 0.0 );
	}
	else if( f_pos.y < 0.75 )
		coord= vec3( f_pos.x * 8.0 - 5.0 , f_pos.y * 8.0 - 5.0, 1.0 );
	else
		coord= vec3( -1.0, f_pos.y * 8.0 - 5.0, f_pos.x * 8.0 - 7.0 );*/

	float a= f_tex_coord.x * pi * 2.0;
	float b= f_tex_coord.y * pi - pi * 0.5;
	coord.y= sin(-b);
	coord.x= coord.z= cos(b);
	coord.x*= sin(a);
	coord.z*= cos(a);

	vec3 tex_data= texture( cubemap, coord ).xyz;
	float mult= 2.0 * texture( cubemap_multipler, coord ).x;
	color= vec4( tex_data * mult, 1.0 );
}