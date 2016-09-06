#include "math_utils.hpp"

m_Vec3 plbProjectPointToPlane( const m_Vec3& point, const m_Vec3& plane_point, const m_Vec3& plane_normal )
{
	const m_Vec3 vec_to_plane_point= point - plane_point;
	const float signed_distance_to_plane= vec_to_plane_point * plane_normal;

	const m_Vec3 projection_point= point - plane_normal * signed_distance_to_plane;

	return projection_point;
}
