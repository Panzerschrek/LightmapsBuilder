uniform samplerCubeShadow cubemap;
uniform float inv_max_light_dst;
uniform vec3 light_pos;
uniform vec3 light_color;
uniform vec3 light_normal;

in vec3 f_pos;
in vec3 f_normal;

out vec4 color;

void main()
{
	vec3 vec_to_light= light_pos - f_pos;
	float vec_to_light_len = length(vec_to_light);
	vec3 normalized_vec_to_light= vec_to_light / vec_to_light_len;

	float angle_scaler= max( 0.0, dot( normalized_vec_to_light, normalize(f_normal) ) );

	angle_scaler*= max( 0.0f, -dot( normalized_vec_to_light, light_normal ) );

	float shadow_factor= texture( cubemap, vec4( normalized_vec_to_light, (vec_to_light_len-0.1)*inv_max_light_dst ) );
	
	color= vec4(
		light_color * ( (shadow_factor * angle_scaler) / (vec_to_light_len * vec_to_light_len) ),
		1.0 );
}
