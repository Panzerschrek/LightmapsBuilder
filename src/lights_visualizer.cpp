#include "shaders_loading.hpp"

#include "lights_visualizer.hpp"


struct Vertex
{
	float pos[3];
	float color[3];
};

static const r_GLSLVersion g_glsl_version( r_GLSLVersion::KnowmNumbers::v430 );
static const float g_directional_lights_distance= 64.0f;

plb_LightsVisualizer::plb_LightsVisualizer(
	const plb_PointLights& point_lights,
	const plb_DirectionalLights& directional_lights,
	const plb_ConeLights& cone_lights,
	const plb_SurfaceSampleLights& surface_sample_lights )
{
	std::vector<Vertex> vertices;
	vertices.reserve(
		point_lights.size() + cone_lights.size() + surface_sample_lights.size() );

	auto push_light=
		[&]( const plb_PointLight& light ) mutable -> void
		{
			vertices.emplace_back();
			Vertex& vertex= vertices.back();

			for( unsigned int i= 0; i < 3; i++ )
			{
				vertex.pos[i]= light.pos[i];
				vertex.color[i]= float(light.color[i]) / 255.0f * light.intensity;
			}
		};

	point_lights_.first_vertex= vertices.size();
	point_lights_.vertex_count= point_lights.size();
	for( const plb_PointLight& light : point_lights )
		push_light( light );

	directional_lights_.first_vertex= vertices.size();
	directional_lights_.vertex_count= directional_lights.size();
	for( const plb_DirectionalLight& light : directional_lights )
	{
		vertices.emplace_back();
		Vertex& vertex= vertices.back();

		for( unsigned int i= 0; i < 3; i++ )
		{
			vertex.pos[i]= light.direction[i] * g_directional_lights_distance;
			vertex.color[i]= float(light.color[i]) / 255.0f * light.intensity;
		}
	}

	cone_lights_.first_vertex= vertices.size();
	cone_lights_.vertex_count= cone_lights.size();
	for( const plb_ConeLight& light : cone_lights )
		push_light( light );

	surface_sample_lights_.first_vertex= vertices.size();
	surface_sample_lights_.vertex_count= surface_sample_lights.size();
	for( const plb_SurfaceSampleLight& light : surface_sample_lights )
		push_light( light );

	vertex_buffer_.VertexData(
		vertices.data(),
		sizeof(Vertex) * vertices.size(),
		sizeof(Vertex) );

	Vertex v;

	vertex_buffer_.VertexAttribPointer(
		0,
		3, GL_FLOAT, false,
		((char*)v.pos) - ((char*)&v) );

	vertex_buffer_.VertexAttribPointer(
		1,
		3, GL_FLOAT, false,
		((char*)v.color) - ((char*)&v) );

	vertex_buffer_.SetPrimitiveType( GL_POINTS );

	shader_.ShaderSource(
		rLoadShader( "light_source_draw_f.glsl", g_glsl_version ),
		rLoadShader( "light_source_draw_v.glsl", g_glsl_version ) );
	shader_.SetAttribLocation( "pos", 0 );
	shader_.SetAttribLocation( "color", 1 );
	shader_.Create();
}

plb_LightsVisualizer::~plb_LightsVisualizer()
{
}

void plb_LightsVisualizer::Draw( const m_Mat4& view_matrix, const m_Vec3& cam_pos )
{
	glEnable( GL_PROGRAM_POINT_SIZE );

	shader_.Bind();
	shader_.Uniform( "view_matrix", view_matrix );
	shader_.Uniform( "sprite_size", 64.0f );

	vertex_buffer_.Bind();

	glDrawArrays( GL_POINTS, point_lights_.first_vertex, point_lights_.vertex_count );
	glDrawArrays( GL_POINTS, cone_lights_.first_vertex, cone_lights_.vertex_count );
	glDrawArrays( GL_POINTS, surface_sample_lights_.first_vertex, surface_sample_lights_.vertex_count );

	m_Mat4 shift_mat;
	shift_mat.Translate( cam_pos );

	shader_.Uniform( "sprite_size", g_directional_lights_distance * 64.0f );
	shader_.Uniform( "view_matrix", shift_mat * view_matrix );

	glDrawArrays( GL_POINTS, directional_lights_.first_vertex, directional_lights_.vertex_count );

	glDisable( GL_PROGRAM_POINT_SIZE );
}
