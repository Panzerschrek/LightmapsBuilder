uniform float inv_max_light_dst;

out vec4 color;

in vec4 f_pos;
in vec3 f_lightmap_coord;

void main()
{
	float l= length(f_pos.xyw);
	gl_FragDepth = min( inv_max_light_dst * l, 1.0 );
	color= vec4 ( l, l, l, 1.0 );
}
