uniform sampler2D tex;
uniform int mip;
uniform float normalizer;

out vec4 color;

void main()
{
	color=
	(
		texelFetch( tex, ivec2( 0, 0 ), mip ) + // z+
		texelFetch( tex, ivec2( 1, 0 ), mip ) + // x+ and x-
		texelFetch( tex, ivec2( 2, 0 ), mip )   // y+ and y-
	) * normalizer / 3.0;
}
