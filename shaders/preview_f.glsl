uniform sampler2DArray lightmap;
uniform sampler2DArray lightmap_test;
uniform samplerCubeShadow cubemap;

uniform sampler2DArray textures[8];

uniform vec3 light_pos;
uniform float inv_max_light_dst= 1.0 / 128.0;

in vec3 f_pos;
in vec3 f_tex_coord;
in vec3 f_lightmap_coord;
in vec3 f_normal;
in float f_texture_array;

out vec4 color;

void main()
{
	vec3 c= texture( textures[int(f_texture_array)], f_tex_coord ).xyz;
	vec3 lightmap_light= texture(lightmap, f_lightmap_coord ).xyz;

	//c= vec3( 0.5, 0.5, 0.5 );
	vec3 linear_color= c * lightmap_light;

	color= vec4( vec3(1.0, 1.0, 1.0) - exp(-2.0 * linear_color), 1.0);
	//color= vec4( linear_color, 1.0 );

	//color= vec4( f_normal * 0.5 + vec3(0.5,0.5,0.5), 1.0 );
}
