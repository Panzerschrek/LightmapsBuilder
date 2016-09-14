#pragma once
#include <vector>

#include <bbox.hpp>
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

	typedef std::vector<unsigned int> SurfacesList;

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

	unsigned int Trace(
		const SurfacesList& surfaces_to_trace,
		const m_Vec3& from,
		const m_Vec3& to,
		TraceResult* out_result= nullptr,
		unsigned int max_result_count= 0 ) const;

	SurfacesList GetPolygonNeighbors(
		const plb_Polygon& polygon,
		const plb_Vertices& polygon_vertices,
		const float threshold ) const;

private:
	struct Surface
	{
		m_Vec3 normal;

		unsigned int first_index;
		unsigned int first_vertex;

		unsigned short index_count;
		unsigned short vertex_count;
	};

	typedef std::vector<Surface> Surfaces;

	typedef m_Vec3 Vertex;
	typedef std::vector<Vertex> Vertices;
	typedef std::vector<unsigned int> Indeces;

	struct GeometrySet
	{
		Surfaces surfaces;
		Vertices vertices;
		Indeces indeces;
	};

	struct TreeNode
	{
		enum class PlaneOrientation
		{
			x, y, z
		};

		static constexpr unsigned short c_no_child= 0xFFFF;

		unsigned int first_surface;
		unsigned int surface_count;

		// 0 - minus, 1 - plus
		unsigned short childs[2];

		PlaneOrientation plane_orientation;
		float dist; // distance to coordinate system center.
	};

	typedef std::vector<TreeNode> Tree;

	struct TraceRequestData
	{
		m_Vec3 from;
		m_Vec3 to;
		m_Vec3 normalized_dir;

		unsigned int result_count;
		unsigned int max_result_count;
		TraceResult* out_result;
	};

private:
	void CheckSurfaceCollision(
		TraceRequestData& data,
		const Surface& surface ) const;

	void CheckCollision_r( TraceRequestData& data, const TreeNode& node ) const;

	m_BBox3 GetSurfaceBBox( const Surface& surface ) const;
	bool BBoxIntersectSurface( const m_BBox3& bbox, const Surface& surface ) const;

	void AddIntersectedSurfacesToList_r(
		const TreeNode& node,
		const m_BBox3& bbox,
		SurfacesList& list ) const;

	void BuildTree();

	void BuildTreeNode_r(
		unsigned int node_index,
		const m_BBox3& node_bounding_box,
		const GeometrySet& in_geometry,
		std::vector<unsigned int>& used_surfaces_indeces,
		TreeNode::PlaneOrientation plane_orientation,
		GeometrySet& out_geometry,
		Tree& out_tree ) const;

private:
	Surfaces surfaces_;
	Vertices vertices_;
	Indeces indeces_;

	Tree tree_;
};
