uniform mat4 view_matrix;

in vec3 pos;
in vec2 tex_coord;
in vec2 lightmap_coord;
in ivec3 tex_maps;
in vec3 normal;

out vec3 f_pos;
out vec3 f_tex_coord;
out float f_texture_array;
out vec3 f_lightmap_coord;
out vec3 f_normal;


const float eps= 0.01;

void main()
{
	f_pos= pos;
	f_tex_coord= vec3( tex_coord, float(tex_maps.y) + eps );
	f_texture_array= float(tex_maps.x) + eps;

	f_lightmap_coord= vec3( lightmap_coord, float(tex_maps.z) + eps );
	f_normal= normal;

	gl_Position= view_matrix * vec4( pos, 1.0 );
}
