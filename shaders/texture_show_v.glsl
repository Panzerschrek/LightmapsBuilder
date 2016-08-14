in vec2 pos;
in vec2 tex_coord;

out vec2 f_pos;
out vec2 f_tex_coord;

void main()
{
	f_pos= pos;
	f_tex_coord= tex_coord;	
	gl_Position= vec4( pos, -0.9999, 1.0 );
}
