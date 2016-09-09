#include "math_utils.hpp"

m_Vec3 plbProjectPointToPlane( const m_Vec3& point, const m_Vec3& plane_point, const m_Vec3& plane_normal )
{
	const m_Vec3 vec_to_plane_point= point - plane_point;
	const float signed_distance_to_plane= vec_to_plane_point * plane_normal;

	const m_Vec3 projection_point= point - plane_normal * signed_distance_to_plane;

	return projection_point;
}

m_Vec3 plbProjectVectorToPlane( const m_Vec3& vector, const m_Vec3& plane_normal )
{
	return vector - plane_normal * ( vector * plane_normal );
}

void plbGetInvLightmapBasisMatrix(
	const m_Vec3& axis_u,
	const m_Vec3& axis_v,
	m_Mat3& out_mat )
{
	out_mat.value[0]= axis_u.x;
	out_mat.value[1]= axis_u.y;
	out_mat.value[2]= axis_u.z;

	out_mat.value[3]= axis_v.x;
	out_mat.value[4]= axis_v.y;
	out_mat.value[5]= axis_v.z;

	const m_Vec3 cross= mVec3Cross( axis_u, axis_v );
	out_mat.value[6]= cross.x;
	out_mat.value[7]= cross.y;
	out_mat.value[8]= cross.z;

	out_mat.Inverse();
}

m_Vec3 plbGetPolygonCenter(
	const plb_Polygon& poly,
	const plb_Vertices& vertices,
	const std::vector<unsigned int>& indeces )
{
	m_Vec3 weighted_triangles_centroid_sum( 0.0f, 0.0f, 0.0f );
	float polygon_area= 0.0f;

	for( unsigned int t= 0; t < poly.index_count; t+= 3 )
	{
		const unsigned int* const index= indeces.data() + poly.first_index + t;

		const m_Vec3 v0( vertices[ index[0] ].pos );
		const m_Vec3 v1( vertices[ index[1] ].pos );
		const m_Vec3 v2( vertices[ index[2] ].pos );

		const m_Vec3 side1= v1 - v0;
		const m_Vec3 side2= v2 - v0;

		const float triangle_area= mVec3Cross( side2, side1 ).Length() * 0.5f;

		weighted_triangles_centroid_sum+= ( v0 + v1 + v2 ) * ( triangle_area / 3.0f );

		polygon_area+= triangle_area;

	} // for polygon triangles

	return weighted_triangles_centroid_sum / polygon_area;
}

float plbGetPolygonArea(
	const plb_Polygon& poly,
	const plb_Vertices& vertices,
	const std::vector<unsigned int>& indeces )
{
	float area= 0.0f;

	for( unsigned int t= 0; t < poly.index_count; t+= 3 )
	{
		const unsigned int* const index= indeces.data() + poly.first_index + t;

		const m_Vec3 v0( vertices[ index[0] ].pos );
		const m_Vec3 v1( vertices[ index[1] ].pos );
		const m_Vec3 v2( vertices[ index[2] ].pos );

		const m_Vec3 side1= v1 - v0;
		const m_Vec3 side2= v2 - v0;

		const float triangle_area= mVec3Cross( side2, side1 ).Length() * 0.5f;
		area+= triangle_area;

	} // for polygon triangles

	return area;
}
