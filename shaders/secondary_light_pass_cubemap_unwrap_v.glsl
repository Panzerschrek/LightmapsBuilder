in vec3 coord; // cubemap vector
in vec2 tex_coord; // target position inside texture [0; 1]

out vec3 f_coord;

void main()
{
	f_coord= coord;
	gl_Position= vec4( tex_coord * 2.0 - vec2( 1.0, 1.0 ), 1.0, 1.0 );
}
