#include <matrix.hpp>
#include <vec.hpp>

#include "formats.hpp"

// Normal must be normalized
m_Vec3 plbProjectPointToPlane( const m_Vec3& point, const m_Vec3& plane_point, const m_Vec3& plane_normal );
m_Vec3 plbProjectVectorToPlane( const m_Vec3& vector, const m_Vec3& plane_normal );

void plbGetInvLightmapBasisMatrix(
	const m_Vec3& axis_u,
	const m_Vec3& axis_v,
	m_Mat3& out_mat );


m_Vec3 plbGetPolygonCenter(
	const plb_Polygon& poly,
	const plb_Vertices& vertices,
	const std::vector<unsigned int>& indeces );

float plbGetPolygonArea(
	const plb_Polygon& poly,
	const plb_Vertices& vertices,
	const std::vector<unsigned int>& indeces );
