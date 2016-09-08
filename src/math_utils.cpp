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
