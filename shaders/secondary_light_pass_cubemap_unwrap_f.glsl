uniform samplerCube cubemap;
uniform samplerCube cubemap_multiplier;

in vec3 f_coord; // Cubemap vector

out vec4 color;

void main()
{
	color=
		texture( cubemap, -f_coord ) *
		texture( cubemap_multiplier, f_coord ).xxxx;
}
