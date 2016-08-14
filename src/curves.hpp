#pragma once

#include "formats.hpp"

void GenCurvesMeshes(
	const std::vector<plb_CurvedSurface>* curves, const std::vector<plb_Vertex>* curves_vertices,
	std::vector<plb_Vertex>* out_vertices, std::vector<unsigned int>* out_indeces, std::vector<plb_Normal>* out_normals );
