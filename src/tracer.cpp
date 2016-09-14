#include <cmath>

#include "curves.hpp"
#include "math_utils.hpp"

#include "tracer.hpp"

static const m_Vec3 g_axis_normals[3]=
{
	{ 1.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f },
};

const float g_length_eps= 1.0f / 8192.0f;
const float g_square_length_eps= g_length_eps * g_length_eps;

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
		const plb_Material& material= level_data.materials[ poly.material_id ];
		if( material.cast_alpha_shadow )
			continue;

		surfaces_.emplace_back();
		Surface& surface= surfaces_.back();

		const unsigned int first_vertex= vertices_.size();
		vertices_.resize( vertices_.size() + poly.vertex_count );
		for( unsigned int v= 0; v < (unsigned int)poly.vertex_count; v++ )
			vertices_[ first_vertex + v ]=
				m_Vec3( level_data.vertices[ poly.first_vertex_number + v ].pos );

		surface.first_index= indeces_.size();
		surface.index_count= poly.index_count;

		surface.first_vertex= first_vertex;
		surface.vertex_count= poly.vertex_count;

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
		const plb_Material& material= level_data.materials[ curve.material_id ];
		if( material.cast_alpha_shadow )
			continue;

		curve_vertices.clear();
		curve_indeces.clear();
		GenCurveMesh( curve, level_data.curved_surfaces_vertices, curve_vertices, curve_indeces, curve_normals );

		vertices_.reserve( vertices_.size() + curve_vertices.size() );
		indeces_.reserve( indeces_.size() + curve_indeces.size() );
		surfaces_.reserve( surfaces_.size() + curve_indeces.size() / 3u );

		for( unsigned int t= 0; t < curve_indeces.size(); t+= 3 )
		{
			const unsigned int* const index= curve_indeces.data() + t;

			const m_Vec3 side0= m_Vec3(curve_vertices[ index[1] ].pos) - m_Vec3(curve_vertices[ index[0] ].pos);
			const m_Vec3 side1= m_Vec3(curve_vertices[ index[2] ].pos) - m_Vec3(curve_vertices[ index[1] ].pos);

			const m_Vec3 normal= mVec3Cross( side1, side0 );
			const float normal_length= normal.Length();
			if( normal_length < 1.0f / (128.0f * 128.0f) )
				continue; // Degenerate triangle - skip it

			surfaces_.emplace_back();
			Surface& surface= surfaces_.back();

			surface.normal= normal / normal_length;

			const unsigned int first_index= indeces_.size();
			indeces_.resize( indeces_.size() + 3u );

			const unsigned int first_vertex= vertices_.size();
			vertices_.resize( vertices_.size() + 3u );

			surface.first_index= first_index;
			surface.index_count= 3u;
			surface.first_vertex= first_vertex;
			surface.vertex_count= 3u;

			// Put triangle vertices and indeces
			for( unsigned int i= 0; i < 3u; i++ )
			{
				indeces_[ first_index + i ]= first_vertex + i;
				vertices_[ first_vertex + i ]= m_Vec3( curve_vertices[ index[i] ].pos );
			}
		} // for curve triangles
	} // for curves

	BuildTree();
}

plb_Tracer::~plb_Tracer()
{
}

unsigned int plb_Tracer::Trace(
	const m_Vec3& from, const m_Vec3& to,
	TraceResult* out_result,
	unsigned int max_result_count ) const
{
	const m_Vec3 dir= to - from;
	const float dir_length= dir.Length();

	TraceRequestData trace_request_data;
	trace_request_data.from= from;
	trace_request_data.to= to;
	trace_request_data.normalized_dir= dir / dir_length;
	trace_request_data.max_result_count= max_result_count;
	trace_request_data.result_count= 0;
	trace_request_data.out_result= out_result;

	const TreeNode* node= &tree_.front();

	unsigned int depth= 0;
	while(1)
	{
		// No childs - return
		if(
			node->childs[0] == TreeNode::c_no_child &&
			node->childs[1] == TreeNode::c_no_child )
			break;

		const m_Vec3& node_normal= g_axis_normals[ size_t(node->plane_orientation) ];
		const float from_pos= node_normal * from - node->dist;
		const float   to_pos= node_normal *   to - node->dist;

		if( from_pos < 0.0f && to_pos < 0.0f )
			node= tree_.data() + node->childs[0];
		else if( from_pos >= 0.0f && to_pos >= 0.0f )
			node= tree_.data() + node->childs[1];
		else
			break;

		// Check this node surfaces if not last node
		for( unsigned int i= node->first_surface; i < node->first_surface + node->surface_count; i++ )
			CheckSurfaceCollision( trace_request_data, surfaces_[i] );

		depth++;
	}

	CheckCollision_r( trace_request_data, *node );

	return trace_request_data.result_count;
}

plb_Tracer::SurfacesList plb_Tracer::GetPolygonNeighbors(
	const plb_Polygon& polygon,
	const plb_Vertices& polygon_vertices,
	const float threshold ) const
{
	SurfacesList result;

	m_BBox3 polygon_bounding_box( plb_Constants::max_vec, plb_Constants::min_vec );

	for( unsigned int v= 0; v < polygon.vertex_count; v++ )
		polygon_bounding_box+= m_Vec3( polygon_vertices[ polygon.first_vertex_number + v ].pos );

	const m_Vec3 threshold_vec( threshold, threshold, threshold );

	polygon_bounding_box.max+= threshold_vec;
	polygon_bounding_box.min-= threshold_vec;

	const TreeNode* node= &tree_.front();
	while(1)
	{
		// No childs - return
		if(
			node->childs[0] == TreeNode::c_no_child &&
			node->childs[1] == TreeNode::c_no_child )
			break;

		const m_Vec3& node_normal= g_axis_normals[ size_t(node->plane_orientation) ];
		const float min_pos= node_normal * polygon_bounding_box.min - node->dist;
		const float max_pos= node_normal * polygon_bounding_box.max - node->dist;

		if( min_pos < 0.0f && max_pos < 0.0f )
			node= tree_.data() + node->childs[0];
		else if( min_pos >= 0.0f && max_pos >= 0.0f )
			node= tree_.data() + node->childs[1];
		else
			break;

		// Check this node surfaces if not last node
		for( unsigned int i= node->first_surface; i < node->first_surface + node->surface_count; i++ )
		{
			if( BBoxIntersectSurface( polygon_bounding_box, surfaces_[i] ) )
				result.push_back(i);
		}
	}

	AddIntersectedSurfacesToList_r( *node, polygon_bounding_box, result );

	return result;
}

void plb_Tracer::CheckSurfaceCollision(
	TraceRequestData& data,
	const Surface& surface ) const
{
	const m_Vec3 vec_to_surface_vertex= vertices_[ indeces_[ surface.first_index ] ] - data.from;

	const float normal_dir_dot= data.normalized_dir * surface.normal;
	if( std::abs(normal_dir_dot) < g_length_eps ) // line paralell to surface plane
		return;

	const float signed_distance_to_surface_plane= vec_to_surface_vertex * surface.normal;

	const m_Vec3 dir_vec_to_plane=
		data.normalized_dir * ( signed_distance_to_surface_plane / normal_dir_dot );

	const m_Vec3 intersection_point= data.from + dir_vec_to_plane;

	const m_Vec3 vec_from_intersection_point_to_point[2]=
	{
		data.from - intersection_point,
		data.to - intersection_point,
	};
	// Intersection point beyond line segment and far from 'from' and 'to' points.
	if(
		vec_from_intersection_point_to_point[0].SquareLength() >= g_square_length_eps &&
		vec_from_intersection_point_to_point[1].SquareLength() >= g_square_length_eps &&
		!( vec_from_intersection_point_to_point[0] * vec_from_intersection_point_to_point[1] <= 0.0f ) )
	{
		return;
	}

	for( unsigned int t= 0; t < surface.index_count; t+= 3 )
	{
		const unsigned int* const index= indeces_.data() + surface.first_index + t;

		if( IsPointInTriangle(
				vertices_[ index[0] ],
				vertices_[ index[1] ],
				vertices_[ index[2] ],
				intersection_point ) )
		{
			data.result_count++;

			if( data.result_count <= data.max_result_count )
			{
				data.out_result[ data.result_count - 1u ].normal= surface.normal;
				data.out_result[ data.result_count - 1u ].pos= intersection_point;
			}

			return;
		}
	} // for surface triangles

}

void plb_Tracer::CheckCollision_r( TraceRequestData& data, const TreeNode& node ) const
{
	for( unsigned int i= node.first_surface; i < node.first_surface + node.surface_count; i++ )
		CheckSurfaceCollision( data, surfaces_[i] );

	for( unsigned int c= 0; c < 2; c++ )
		if( node.childs[c] != TreeNode::c_no_child )
			CheckCollision_r( data, tree_[ node.childs[c] ] );
}

m_BBox3 plb_Tracer::GetSurfaceBBox( const Surface& surface ) const
{
	m_BBox3 box( plb_Constants::max_vec, plb_Constants::min_vec );

	for( unsigned int i= 0; i < surface.vertex_count; i++ )
		box+= vertices_[ surface.first_vertex + i ];

	return box;
}

bool plb_Tracer::BBoxIntersectSurface( const m_BBox3& bbox, const Surface& surface ) const
{
	const m_BBox3 surface_bbox= GetSurfaceBBox( surface );

	for( unsigned int i= 0; i < 3; i++ )
	{
		if(
			surface_bbox.max.ToArr()[i] < bbox.min.ToArr()[i] ||
			bbox.max.ToArr()[i] < surface_bbox.min.ToArr()[i] )
			return false;
	}

	return true;
}

void plb_Tracer::AddIntersectedSurfacesToList_r(
	const TreeNode& node,
	const m_BBox3& bbox,
	SurfacesList& list ) const
{
	for( unsigned int i= node.first_surface; i < node.first_surface + node.surface_count; i++ )
	{
		if( BBoxIntersectSurface( bbox, surfaces_[i] ) )
			list.push_back(i);
	}

	for( unsigned int c= 0; c < 2; c++ )
		if( node.childs[c] != TreeNode::c_no_child )
			AddIntersectedSurfacesToList_r( tree_[ node.childs[c] ], bbox, list );
}

void plb_Tracer::BuildTree()
{
	GeometrySet geometry;
	geometry.surfaces= std::move(surfaces_);
	geometry.vertices= std::move(vertices_);
	geometry.indeces= std::move(indeces_);

	tree_.emplace_back();

	// Calculate bounding box
	m_BBox3 bounding_box( plb_Constants::max_vec, plb_Constants::min_vec );

	for( const Vertex& vertex : geometry.vertices )
		bounding_box+= vertex;

	// round bounding box coordinates
	for( unsigned int i= 0; i < 3; i++ )
	{
		bounding_box.min.ToArr()[i]= std::ceil ( bounding_box.min.ToArr()[i] );
		bounding_box.max.ToArr()[i]= std::floor( bounding_box.max.ToArr()[i] );
	}

	std::vector<unsigned int> used_surfaces_indeces( geometry.surfaces.size() );
	for( unsigned int& index : used_surfaces_indeces )
		index= &index - used_surfaces_indeces.data();

	GeometrySet result_geometry;

	BuildTreeNode_r(
		0,
		bounding_box,
		geometry,
		used_surfaces_indeces,
		TreeNode::PlaneOrientation::z,
		result_geometry,
		tree_ );

	surfaces_= std::move( result_geometry.surfaces );
	vertices_= std::move( result_geometry.vertices );
	indeces_= std::move( result_geometry.indeces );
}

void plb_Tracer::BuildTreeNode_r(
	unsigned int node_index,
	const m_BBox3& node_bounding_box,
	const GeometrySet& in_geometry,
	std::vector<unsigned int>& used_surfaces_indeces,
	TreeNode::PlaneOrientation plane_orientation,
	GeometrySet& out_geometry,
	Tree& out_tree ) const
{
	// TODO - profile this
	const unsigned int c_min_surfaces_for_node= 16;

	auto insert_surface=
	[&]( const Surface& in_surface ) mutable -> void
	{
		out_geometry.surfaces.emplace_back();
		Surface& out_surface= out_geometry.surfaces.back();

		out_surface.normal= in_surface.normal;
		out_surface.index_count= in_surface.index_count;
		out_surface.vertex_count= in_surface.vertex_count;

		const unsigned int first_index= out_geometry.indeces.size();
		const unsigned int first_vertex= out_geometry.vertices.size();
		out_geometry.indeces.resize( first_index + out_surface.index_count );
		out_geometry.vertices.resize( first_vertex + out_surface.vertex_count );

		for( unsigned int i= 0; i < out_surface.index_count; i++ )
			out_geometry.indeces[ first_index + i ]=
				in_geometry.indeces[ in_surface.first_index + i ] -
				in_surface.first_vertex + first_vertex;

		for( unsigned int v= 0; v < out_surface.vertex_count; v++ )
			out_geometry.vertices[ first_vertex + v ]=
				in_geometry.vertices[ in_surface.first_vertex + v ];

		out_surface.first_index= first_index;
		out_surface.first_vertex= first_vertex;
	};

	TreeNode* node= out_tree.data() + node_index;

	const m_Vec3& node_plane_normal= g_axis_normals[ size_t(plane_orientation) ];

	node->plane_orientation= plane_orientation;
	node->dist= node_plane_normal * node_bounding_box.Center();

	// Leaf
	if( used_surfaces_indeces.size() < c_min_surfaces_for_node )
	{
		node->childs[0]= node->childs[1]= TreeNode::c_no_child;
		node->first_surface= out_geometry.surfaces.size();
		node->surface_count= used_surfaces_indeces.size();

		for( unsigned int i= 0; i < node->surface_count; i++ )
			insert_surface( in_geometry.surfaces[ used_surfaces_indeces[i] ] );
	}
	else // Node
	{
		std::vector<unsigned int> child_surfaces_indeces[2];

		node->first_surface= out_geometry.surfaces.size();
		node->surface_count= 0;
		node->childs[0]= out_tree.size();
		node->childs[1]= out_tree.size() + 1;

		out_tree.resize( out_tree.size() + 2 );
		// Update pointer after vector resize.
		node= out_tree.data() + node_index;

		for( const unsigned int in_surface_index : used_surfaces_indeces )
		{
			unsigned int minus_vertex_count= 0;
			unsigned int plus_vertex_count= 0;

			const Surface& surface= in_geometry.surfaces[ in_surface_index ];
			for( unsigned int i= 0; i < surface.index_count; i++ )
			{
				const Vertex& vertex=
					in_geometry.vertices[ in_geometry.indeces[ surface.first_index + i ] ];

				const float signed_distance_to_node_plane=
					vertex * node_plane_normal - node->dist;

				if( signed_distance_to_node_plane < 0.0f )
					minus_vertex_count++;
				else
					plus_vertex_count++;

			} // for surface indeces

			if( minus_vertex_count == surface.index_count )
				child_surfaces_indeces[0].push_back( in_surface_index );
			else if( plus_vertex_count == surface.index_count )
				child_surfaces_indeces[1].push_back( in_surface_index );
			else
			{
				// Surface splitted by node plane - place it in this node.
				insert_surface( surface );
				node->surface_count++;
			}

		} // for input surfaces

		const TreeNode::PlaneOrientation child_planes_orientation=
			TreeNode::PlaneOrientation( ( size_t(plane_orientation) + 1u ) % 3 );

		for( unsigned int i= 0; i < 2; i++ )
		{
			m_BBox3 child_box= node_bounding_box;
			( i == 0 ? child_box.max : child_box.min )
				.ToArr()[ size_t(plane_orientation) ]= node->dist;

			BuildTreeNode_r(
				node->childs[i],
				child_box,
				in_geometry,
				child_surfaces_indeces[i],
				child_planes_orientation,
				out_geometry,
				out_tree );

			// Update pointer after recursive call
			node= out_tree.data() + node_index;
		} // for childs
	} // if node
}
