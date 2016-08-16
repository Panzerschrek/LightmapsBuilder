uniform sampler2DShadow shadowmap;
uniform mat4 view_matrix;
uniform vec3 light_pos;
uniform vec3 light_color;

in vec3 f_pos;
in vec3 f_normal;

out vec4 color;

void main()
{
	vec3 vec_to_light= light_pos - f_pos;
	float vec_to_light_len = length(vec_to_light);
	vec3 normalized_vec_to_light= vec_to_light / vec_to_light_len;

	float angle_scaler= max( 0.0, dot( normalized_vec_to_light, normalize(f_normal) ) );

	float offset= 0.05 / min( 10.0, max( angle_scaler, 0.01 ) );
	vec3 pos_m= f_pos + normalized_vec_to_light * offset;

	vec4 shadow_pos= ( view_matrix * vec4( pos_m, 1.0 ) );
	shadow_pos.xyz/= shadow_pos.w;
	float shadow_factor=
		texture(
			shadowmap,
			shadow_pos.xyz * vec3( 0.5, 0.5, 0.5 ) + vec3( 0.5, 0.5, 0.5 ) );

	//cull cone light back
	shadow_factor*= step(0.0, shadow_pos.w);
	// Do not lit outside cone
	shadow_factor*= 1.0 - smoothstep(0.8 * 0.8, 1.0, dot(shadow_pos.xy, shadow_pos.xy ) );

	color= vec4(
		light_color * ( (shadow_factor * angle_scaler) / (vec_to_light_len * vec_to_light_len) ),
		1.0 );
}
