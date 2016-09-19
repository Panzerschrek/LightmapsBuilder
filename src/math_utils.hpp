#include <cmath>
#include <limits>

#include <matrix.hpp>
#include <vec.hpp>

#include "formats.hpp"

namespace plb_Constants
{

const float pi= 3.1415926535f;
const float inv_pi= 1.0f / pi;

const float half_pi= 0.5f * pi;
const float two_pi= 2.0f * pi;

const float to_rad= pi / 180.0f;
const float to_deg= 180.0f / pi;

const float sqrt_2= std::sqrt( 2.0f );

const float min_float= std::numeric_limits<float>::lowest();
const float max_float= std::numeric_limits<float>::max();

const m_Vec3 min_vec( min_float, min_float, min_float );
const m_Vec3 max_vec( max_float, max_float, max_float );

} // namespace plb_Contants

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
