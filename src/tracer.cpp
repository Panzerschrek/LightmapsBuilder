#include <cmath>

#include "curves.hpp"

#include "tracer.hpp"

static bool IsPointInTriangle(
	const m_Vec3& v0, const m_Vec3& v1, const m_Vec3& v2,
	const m_Vec3& point )
{
	m_Vec3 cross[3];

	cross[0]= mVec3Cross( point - v0, v1 - v0 );
	cross[1]= mVec3Cross( point - v1, v2 - v1 );
	cross[2]= mVec3Cross( point - v2, v0 - v2 );

	float dot[2];
	dot[0]= cross[0] * cross[1];
	dot[1]= cross[0] * cross[2];

	return dot[0] > 0.0f && dot[1] > 0.0f;
}

plb_Tracer::plb_Tracer( const plb_LevelData& level_data )
{
	// Convert input polygons to more compact format
	surfaces_.reserve( level_data.polygons.size() );
	for( const plb_Polygon& poly : level_data.polygons )
	{
		surfaces_.emplace_back();
		Surface& surface= surfaces_.back();

		const unsigned int first_vertex= vertices_.size();
		vertices_.resize( vertices_.size() + poly.vertex_count );
		for( unsigned int v= 0; v < (unsigned int)poly.vertex_count; v++ )
			vertices_[ first_vertex + v ]=
				m_Vec3( level_data.vertices[ poly.first_vertex_number + v ].pos );

		surface.first_index= indeces_.size();
		surface.index_count= poly.index_count;

		// Add and correct indeces
		indeces_.resize( indeces_.size() + poly.index_count );
		unsigned int* const index= indeces_.data() + indeces_.size() - poly.index_count;
		for( unsigned int i= 0; i < (unsigned int)poly.index_count; i++ )
			index[i]= level_data.polygons_indeces[ poly.first_index + i ] - poly.first_vertex_number + first_vertex;

		surface.normal= m_Vec3( poly.normal );
		surface.normal.Normalize();
	} // for polygons

	plb_Vertices curve_vertices;
	std::vector<unsigned int> curve_indeces;
	plb_Normals curve_normals;
	for( const plb_CurvedSurface& curve :level_data.curved_surfaces )
	{
		const unsigned int first_vertex= vertices_.size();

		curve_vertices.clear();
		curve_indeces.clear();

		GenCurveMesh( curve, level_data.curved_surfaces_vertices, curve_vertices, curve_indeces, curve_normals );

		vertices_.reserve( vertices_.size() + curve_vertices.size() );
		for( const plb_Vertex& curve_vertex : curve_vertices )
			vertices_.emplace_back( curve_vertex.pos );

		indeces_.reserve( indeces_.size() + curve_indeces.size() );
		surfaces_.reserve( surfaces_.size() + curve_indeces.size() / 3 );
		for( unsigned int t= 0; t < curve_indeces.size(); t+= 3 )
		{
			surfaces_.emplace_back();
			Surface& surface= surfaces_.back();

			const unsigned int* const index= curve_indeces.data() + t;

			const m_Vec3 side0= m_Vec3(curve_vertices[ index[1] ].pos) - m_Vec3(curve_vertices[ index[0] ].pos);
			const m_Vec3 side1= m_Vec3(curve_vertices[ index[2] ].pos) - m_Vec3(curve_vertices[ index[1] ].pos);

			surface.normal= mVec3Cross( side0, side1 );
			surface.normal.Normalize();

			unsigned int first_index= indeces_.size();

			surface.first_index= first_index;
			surface.index_count= 3;

			for( unsigned int i= 0; i < 3; i++ )
				indeces_.push_back( index[i] + first_vertex );

		} // for curve triangles
	} // for curves
}

plb_Tracer::~plb_Tracer()
{
}

bool plb_Tracer::Trace( const m_Vec3& from, const m_Vec3& to ) const
{
	const float c_length_eps= 0.0001f;

	const m_Vec3 dir= to - from;
	const float dir_length= dir.Length();

	if( dir_length <= c_length_eps )
		return false;

	const m_Vec3 dir_normalized= dir / dir_length;

	for( const Surface& surface : surfaces_ )
	{
		const m_Vec3 vec_to_surface_vertex= vertices_[ indeces_[ surface.first_index ] ] - from;

		const float normal_dir_dot= dir_normalized * surface.normal;
		if( std::abs(normal_dir_dot) < c_length_eps ) // line parralell to surface plane
			continue;

		const float signed_distance_to_surface_plane= vec_to_surface_vertex * surface.normal;

		const m_Vec3 dir_vec_to_plane=
			dir_normalized * ( signed_distance_to_surface_plane / normal_dir_dot );

		const m_Vec3 intersection_point= from + dir_vec_to_plane;

		const m_Vec3 vec_from_intersection_point_to_point[2]=
		{
			from - intersection_point,
			to - intersection_point,
		};
		// Intersection point beyond line segment
		if( !( vec_from_intersection_point_to_point[0] * vec_from_intersection_point_to_point[1] <= 0.0f ) )
			continue;

		for( unsigned int t= 0; t < surface.index_count; t+= 3 )
		{
			const unsigned int* const index= indeces_.data() + surface.first_index + t;

			if( IsPointInTriangle(
					vertices_[ index[0] ],
					vertices_[ index[1] ],
					vertices_[ index[2] ],
					intersection_point ) )
			{
				return true;
			}

		} // for surface triangles
	} // for surfaces

	return false;
}
