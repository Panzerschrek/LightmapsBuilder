uniform vec2 tex_coord;

void main()
{
	gl_Position= vec4( tex_coord, 1.0, 1.0 );
}
