uniform mat4 view_matrix;
uniform float sprite_size;

in vec3 pos;
in vec3 color;

out vec3 f_color;

void main()
{
	f_color= color;

	vec4 pos_projected= view_matrix * vec4( pos, 1.0 );

	gl_PointSize= max( 1.0, sprite_size * 0.5 / max( 1.0, pos_projected.w ) );
	gl_Position= pos_projected;
}
