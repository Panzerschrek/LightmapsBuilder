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

	struct BoundingBox
	{
		m_Vec3 min;
		m_Vec3 max;
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

	void BuildTree();

	void BuildTreeNode_r(
		unsigned int node_index,
		const BoundingBox& node_bounding_box,
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
