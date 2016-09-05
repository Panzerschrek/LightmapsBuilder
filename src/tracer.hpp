#pragma once
#include <vector>

#include <vec.hpp>

#include "formats.hpp"

class plb_Tracer final
{
public:
	struct TraceResult
	{
		m_Vec3 pos;
		m_Vec3 normal;
	};

	explicit plb_Tracer( const plb_LevelData& level_data );
	~plb_Tracer();

	// Found intersections between line segment and level geometry.
	// Returns number of intersections.
	// Result intersections placed into out_result, but no more, than max_result_count.
	unsigned int Trace(
		const m_Vec3& from,
		const m_Vec3& to,
		TraceResult* out_result= nullptr,
		unsigned int max_result_count= 0 ) const;

private:
	struct Surface
	{
		unsigned int first_index;
		unsigned int index_count;

		m_Vec3 normal;
	};

	typedef std::vector<Surface> Surfaces;

	typedef m_Vec3 Vertex;
	typedef std::vector<Vertex> Vertices;
	typedef std::vector<unsigned int> Indeces;

private:
	Surfaces surfaces_;
	Vertices vertices_;
	Indeces indeces_;
};
