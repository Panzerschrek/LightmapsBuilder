#pragma once
#include <vector>

#include <vec.hpp>

#include "formats.hpp"

class plb_Tracer final
{
public:
	explicit plb_Tracer( const plb_LevelData& level_data );
	~plb_Tracer();

	bool Trace( const m_Vec3& from, const m_Vec3& to ) const;

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
