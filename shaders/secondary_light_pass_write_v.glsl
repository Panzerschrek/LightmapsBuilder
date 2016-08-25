uniform vec3 tex_coord;

out vec3 g_coord;

void main()
{
	g_coord= vec3( tex_coord.xy * 2.0 - vec2( 1.0, 1.0 ), tex_coord.z );
}
