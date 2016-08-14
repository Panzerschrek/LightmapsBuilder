#version 430

uniform mat4 view_matrix;
uniform vec4 clip_planes[4];

in vec3 pos;
in vec2 tex_coord;
in vec2 lightmap_coord;
in vec3 tex_maps;
in vec3 normal;

void main()
{
	gl_Position= view_matrix * vec4( pos, 1.0 );
}
