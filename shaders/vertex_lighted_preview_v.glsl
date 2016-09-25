uniform mat4 view_matrix;

uniform sampler2DArray lightmap;
uniform sampler2DArray secondary_lightmap;

uniform float primary_lightmap_scaler;
uniform float secondary_lightmap_scaler;

uniform vec2 secondaty_lightmap_tex_coord_scaler;

in vec3 pos;
in vec2 tex_coord;
in vec2 lightmap_coord;
in ivec3 tex_maps;
in vec3 normal;

out vec3 f_pos;
out vec3 f_tex_coord;
out float f_texture_array;
out vec3 f_light;
out vec3 f_normal;

const float eps= 0.01;

void main()
{
	f_pos= pos;
	f_tex_coord= vec3( tex_coord, float(tex_maps.y) + eps );
	f_texture_array= float(tex_maps.x) + eps;

	vec3 lightmap_coord_3d= vec3( lightmap_coord, float(tex_maps.z) + eps );
	vec3 secondary_lightmap_coord=
		vec3( lightmap_coord_3d.xy * secondaty_lightmap_tex_coord_scaler, lightmap_coord_3d.z );

	f_light=
		texture(lightmap, lightmap_coord_3d ).xyz * primary_lightmap_scaler +
		texture( secondary_lightmap, secondary_lightmap_coord ).xyz * secondary_lightmap_scaler;

	f_normal= normal;

	gl_Position= view_matrix * vec4( pos, 1.0 );
}
