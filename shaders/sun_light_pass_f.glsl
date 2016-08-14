#version 400

uniform sampler2DShadow shadowmap;
uniform mat4 view_matrix;
uniform vec3 light_dir;
uniform vec3 light_color;

in vec3 f_pos;
in vec3 f_normal;

out vec4 color;

void main()
{
	vec3 normalized_normal= normalize(f_normal);
	float normal_vec_to_light_cos= dot(normalized_normal, light_dir);

	float offset= 0.05 / min( 10.0, normal_vec_to_light_cos );
	vec3 pos_m= f_pos + light_dir * offset;

	vec3 shadow_pos= ( view_matrix * vec4( pos_m, 1.0 ) ).xyz;
	shadow_pos= shadow_pos * 0.5 + vec3( 0.5, 0.5, 0.5 );
	float shadow_factor= texture( shadowmap, shadow_pos );

	color= vec4( (normal_vec_to_light_cos * shadow_factor) * light_color, 1.0 );
}
