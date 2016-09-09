uniform samplerCube cubemap;
uniform samplerCube cubemap_multiplier;
uniform sampler2D tex;

in vec2 f_pos;
in vec2 f_tex_coord;

out vec4 color;

const float pi= 3.1415926535;

void main()
{
	color= texture( tex, f_tex_coord );
}
