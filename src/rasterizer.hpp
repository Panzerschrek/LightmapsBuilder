#pragma once

#include <vec.hpp>

template<class RasterElement>
class plb_Rasterizer final
{
public:
	struct Buffer final
	{
		unsigned int size[2];
		RasterElement* data;
	};

	plb_Rasterizer( const Buffer& buffer );

	void DrawTriangle(
		const m_Vec2* vertices,
		const RasterElement* vertex_attributes );

private:
	void DrawTrianglePart();

private:
	const Buffer buffer_;

	// DrawTrianglePart parameters
	// 0 - lower left
	// 1 - upper left
	// 2 - lower right
	// 3 - upper right
	const m_Vec2* triangle_part_vertices_[4];
	const RasterElement* triangle_part_attributes_[4];
};


#include "rasterizer.inl"
