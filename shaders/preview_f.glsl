uniform sampler2DArray lightmap;
uniform sampler2DArray secondary_lightmap;
uniform sampler2DArray lightmap_test;
uniform samplerCubeShadow cubemap;

uniform float primary_lightmap_scaler;
uniform float secondary_lightmap_scaler;

uniform vec2 secondaty_lightmap_tex_coord_scaler;

uniform sampler2DArray textures[8];

in vec3 f_pos;
in vec3 f_tex_coord;
in vec3 f_lightmap_coord;
in vec3 f_normal;
in float f_texture_array;

out vec4 color;

void main()
{
	vec3 c= texture( textures[int(f_texture_array)], f_tex_coord ).xyz;

	vec3 secondary_lightmap_coord=
		vec3( f_lightmap_coord.xy * secondaty_lightmap_tex_coord_scaler, f_lightmap_coord.z );

	vec3 lightmap_light=
		texture(lightmap, f_lightmap_coord ).xyz * primary_lightmap_scaler +
		texture( secondary_lightmap, secondary_lightmap_coord ).xyz * secondary_lightmap_scaler;

	c= vec3( 1.0, 1.0, 1.0 );
	vec3 linear_color= c * lightmap_light;

	//color= vec4( vec3(1.0, 1.0, 1.0) - exp(-0.1 * linear_color), 1.0);
	color= vec4( linear_color * 0.02, 1.0 );

	//color= vec4( f_normal * 0.5 + vec3(0.5,0.5,0.5), 1.0 );
}
