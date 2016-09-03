uniform mat4 view_matrix;
uniform vec4 clip_planes[4];

in vec3 pos;
in vec2 tex_coord;
in vec2 lightmap_coord;
in ivec3 tex_maps;
in vec3 normal;

out vec3 f_tex_coord;
out float f_texture_array;

void main()
{
	f_tex_coord= vec3( tex_coord, float(tex_maps.y) + 0.01 );
	f_texture_array= float(tex_maps.x) + 0.01;

	gl_Position= view_matrix * vec4( pos, 1.0 );
}
