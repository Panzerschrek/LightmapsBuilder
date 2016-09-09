in vec3 f_color;

out vec4 color;

void main()
{
	vec2 pos= gl_PointCoord.xy * 2.0 - vec2( 1.0, 1.0 );
	if( dot( pos, pos ) >= 1.0 )
		discard;

	color= vec4( f_color, 1.0 );
}
