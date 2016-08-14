#version 400

uniform mat4 view_matrix;

in vec3 pos;

void main()
{
	gl_Position= view_matrix * vec4( pos, 1.0 );
}
