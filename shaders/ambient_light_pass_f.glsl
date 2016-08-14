#version 400

uniform vec3 ambient_light_color;

out vec4 color;

void main()
{	
	color= vec4( ambient_light_color, 1.0);
}