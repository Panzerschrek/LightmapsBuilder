#pragma once

#include <vec.hpp>

#include "formats.hpp"

void GenCurveMesh(
	const plb_CurvedSurface& curve,
	const plb_Vertices& curves_vertices,
	plb_Vertices& out_vertices, std::vector<unsigned int>& out_indeces, plb_Normals& out_normals );

void GenCurvesMeshes(
	const plb_CurvedSurfaces& curves, const plb_Vertices& curves_vertices,
	plb_Vertices& out_vertices, std::vector<unsigned int>& out_indeces, plb_Normals& out_normals );

struct PositionAndNormal
{
	m_Vec3 pos;
	m_Vec3 normal;
};

void CalculateCurveCoordinatesForLightTexels(
	const plb_CurvedSurface& curve,
	const m_Vec2& lightmap_coord_scaler, const m_Vec2& lightmap_coord_shift,
	const unsigned int* lightmap_size,
	const plb_Vertices& vertices,
	PositionAndNormal* out_coordinates );
