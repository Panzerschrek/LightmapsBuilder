#pragma once

#include <glsl_program.hpp>
#include <matrix.hpp>
#include <polygon_buffer.hpp>
#include <vec.hpp>

#include "formats.hpp"

class plb_LightsVisualizer final
{
public:
	plb_LightsVisualizer(
		const plb_PointLights& point_lights,
		const plb_DirectionalLights& directional_lights,
		const plb_ConeLights& cone_lights,
		const plb_SurfaceSampleLights& surface_sample_lights );

	~plb_LightsVisualizer();

	void Draw( const m_Mat4& view_matrix, const m_Vec3& cam_pos );

private:
	struct BufferPart
	{
		unsigned int first_vertex;
		unsigned int vertex_count;
	};

private:
	r_GLSLProgram shader_;
	r_PolygonBuffer vertex_buffer_;

	BufferPart point_lights_;
	BufferPart directional_lights_;
	BufferPart cone_lights_;
	BufferPart surface_sample_lights_;
};
