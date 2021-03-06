#include <cmath>
#include <cstring>
#include <iostream>

#include <vec.hpp>

#include "math_utils.hpp"
#include "rasterizer.hpp"

#include "curves.hpp"

static const float g_max_angle_rad= 8.0f * plb_Constants::to_rad;
static const unsigned int g_max_subdivisions= 63;
static const float g_normal_len_eps= 1e-3f;

static void GetControlPointsWeights( float kx, float ky, float kx1, float ky1, float* out_weights )
{
	out_weights[0]= ky1 * ky1 * kx1 * kx1;
	out_weights[1]= ky1 * ky1 * kx  * kx1 * 2.0f;
	out_weights[2]= ky1 * ky1 * kx  * kx;
	out_weights[3]= ky  * ky1 * kx1 * kx1 * 2.0f;
	out_weights[4]= ky  * ky1 * kx  * kx1 * 4.0f;
	out_weights[5]= ky  * ky1 * kx  * kx  * 2.0f;
	out_weights[6]= ky  * ky  * kx1 * kx1;
	out_weights[7]= ky  * ky  * kx  * kx1 * 2.0f;
	out_weights[8]= ky  * ky  * kx  * kx;
}

void GetPatchSubdivisions( const m_Vec3* control_vertices, float max_angle_rad, unsigned int* out_subdivisions )
{
	const float eps= 1e-3f;

	float max_angle_x= 0.0f, max_angle_y= 0.0f;
	for( unsigned int i= 0; i< 3; i++ )
	{
		const m_Vec3 vec0= control_vertices[i*3+1] - control_vertices[i*3+0];
		const m_Vec3 vec1= control_vertices[i*3+2] - control_vertices[i*3+1];
		const float l0= vec0.Length();
		const float l1= vec1.Length();
		if( l0 > eps && l1 > eps )
		{
			const float dot= (vec0 * vec1) / ( l0 * l1 );
			const float angle= acosf(dot);
			if( angle > max_angle_x ) max_angle_x= angle;
		}
	}
	for( unsigned int i= 0; i< 3; i++ )
	{
		const m_Vec3 vec0= control_vertices[3+i] - control_vertices[i+0];
		const m_Vec3 vec1= control_vertices[6+i] - control_vertices[3+i];
		const float l0= vec0.Length();
		const float l1= vec1.Length();
		if( l0 > eps && l1 > eps )
		{
			const float dot= (vec0 * vec1) / ( l0 * l1 );
			const float angle= acosf(dot);
			if( angle > max_angle_y ) max_angle_y= angle;
		}
	}

	out_subdivisions[0]= (unsigned int) std::ceil( max_angle_x / max_angle_rad );
	if( out_subdivisions[0] < 1 ) out_subdivisions[0]= 1;
	if( out_subdivisions[0] > g_max_subdivisions ) out_subdivisions[0]= g_max_subdivisions;
	out_subdivisions[1]= (unsigned int) std::ceil( max_angle_y / max_angle_rad );
	if( out_subdivisions[1] < 1 ) out_subdivisions[1]= 1;
	if( out_subdivisions[1] > g_max_subdivisions ) out_subdivisions[1]= g_max_subdivisions;
}

static inline m_Vec3 GenCurveNormal( const m_Vec3* base_vertices, float kx, float ky, float kx1, float ky1 )
{
	// Try calculate normal, using derivatives of spline function.
	// normal = cross( d_pos / du, d_pos / dv )
	// If result normal has zero length - it is fine.
	// Result normal in fragment will be interpolated across three vertices, at least one of them should have non-zero normal.

	float d_pos_dx_kx[3];
	d_pos_dx_kx[0]= 2.0f * kx - 2.0f;
	d_pos_dx_kx[1]= 2.0f - 4.0f * kx;
	d_pos_dx_kx[2]= 2.0f * kx;
	const m_Vec3 d_pos_dx=
		ky1 *
			( base_vertices[0]*d_pos_dx_kx[0] + base_vertices[1]*d_pos_dx_kx[1] + base_vertices[2]*d_pos_dx_kx[2] ) +
		2.0f * ky * ky1 *
			( base_vertices[3]*d_pos_dx_kx[0] + base_vertices[4]*d_pos_dx_kx[1] + base_vertices[5]*d_pos_dx_kx[2] ) +
		ky * ky *
			( base_vertices[6]*d_pos_dx_kx[0] + base_vertices[7]*d_pos_dx_kx[1] + base_vertices[8]*d_pos_dx_kx[2] );

	d_pos_dx_kx[0]= kx1 * kx1;
	d_pos_dx_kx[1]= 2.0f * kx * kx1;
	d_pos_dx_kx[2]= kx * kx;
	const m_Vec3 d_pos_dy=
		( 2.0f * ky - 2.0f ) *
			( base_vertices[0]*d_pos_dx_kx[0] + base_vertices[1]*d_pos_dx_kx[1] + base_vertices[2]*d_pos_dx_kx[2] ) +
		( 2.0f - 4.0f * ky ) *
			( base_vertices[3]*d_pos_dx_kx[0] + base_vertices[4]*d_pos_dx_kx[1] + base_vertices[5]*d_pos_dx_kx[2] ) +
		2.0f * ky *
			( base_vertices[6]*d_pos_dx_kx[0] + base_vertices[7]*d_pos_dx_kx[1] + base_vertices[8]*d_pos_dx_kx[2] );

	return mVec3Cross( d_pos_dy, d_pos_dx );
}

void GenCurveMesh(
	const plb_CurvedSurface& curve,
	const plb_Vertices& curves_vertices,
	plb_Vertices& out_vertices, std::vector<unsigned int>& out_indeces, plb_Normals& out_normals )
{
	const plb_Vertex* v_p= curves_vertices.data();

	for( unsigned int y= 0; y< curve.grid_size[1]-1; y+=2 )
	for( unsigned int x= 0; x< curve.grid_size[0]-1; x+=2 ) // for curve patches
	{
		const m_Vec3 base_vertices[9]=
		{
			m_Vec3( v_p[ curve.first_vertex_number + x   +  y    * curve.grid_size[0] ].pos ),
			m_Vec3( v_p[ curve.first_vertex_number + x+1 +  y    * curve.grid_size[0] ].pos ),
			m_Vec3( v_p[ curve.first_vertex_number + x+2 +  y    * curve.grid_size[0] ].pos ),
			m_Vec3( v_p[ curve.first_vertex_number + x   + (y+1) * curve.grid_size[0] ].pos ),
			m_Vec3( v_p[ curve.first_vertex_number + x+1 + (y+1) * curve.grid_size[0] ].pos ),
			m_Vec3( v_p[ curve.first_vertex_number + x+2 + (y+1) * curve.grid_size[0] ].pos ),
			m_Vec3( v_p[ curve.first_vertex_number + x   + (y+2) * curve.grid_size[0] ].pos ),
			m_Vec3( v_p[ curve.first_vertex_number + x+1 + (y+2) * curve.grid_size[0] ].pos ),
			m_Vec3( v_p[ curve.first_vertex_number + x+2 + (y+2) * curve.grid_size[0] ].pos ),
		};
		const m_Vec2 base_vertices_lightmap_coords[9]=
		{
			m_Vec2( v_p[ curve.first_vertex_number + x   +  y    * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+1 +  y    * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+2 +  y    * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x   + (y+1) * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+1 + (y+1) * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+2 + (y+1) * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x   + (y+2) * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+1 + (y+2) * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+2 + (y+2) * curve.grid_size[0] ].lightmap_coord ),
		};
		const m_Vec2 base_vertices_texture_coords[9]=
		{
			m_Vec2( v_p[ curve.first_vertex_number + x   +  y    * curve.grid_size[0] ].tex_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+1 +  y    * curve.grid_size[0] ].tex_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+2 +  y    * curve.grid_size[0] ].tex_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x   + (y+1) * curve.grid_size[0] ].tex_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+1 + (y+1) * curve.grid_size[0] ].tex_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+2 + (y+1) * curve.grid_size[0] ].tex_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x   + (y+2) * curve.grid_size[0] ].tex_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+1 + (y+2) * curve.grid_size[0] ].tex_coord ),
			m_Vec2( v_p[ curve.first_vertex_number + x+2 + (y+2) * curve.grid_size[0] ].tex_coord ),
		};

		const unsigned int base_vertex_index= curve.first_vertex_number + x +  y * curve.grid_size[0];
		unsigned char tex_maps[4];
		std::memcpy( tex_maps, v_p[ base_vertex_index ].tex_maps, 4 );

		unsigned int subdivisions[2];
		GetPatchSubdivisions( base_vertices, g_max_angle_rad, subdivisions );

		const unsigned int patch_gird_size[2]= { subdivisions[0]+1, subdivisions[1]+1 };
		const unsigned int vertex_count= patch_gird_size[0] * patch_gird_size[1];
		const unsigned int first_out_vertex_index= out_vertices.size();

		out_vertices.resize( out_vertices.size() + vertex_count );
		plb_Vertex* const vert= out_vertices.data() + first_out_vertex_index;

		out_normals.resize( out_normals.size() + vertex_count );
		plb_Normal* const normals= out_normals.data() + out_normals.size() - vertex_count;

		float ky= 0.0f, dky= 1.0f / float(subdivisions[1]);
		for( unsigned int z= 0; z< patch_gird_size[1]; z++, ky+= dky )
		{
			float ky1= 1.0f - ky;
			float kx= 0.0f, dkx= 1.0f / float(subdivisions[0]);
			for( unsigned int w= 0; w< patch_gird_size[0]; w++, kx+= dkx )
			{
				const float kx1= 1.0f - kx;
				float vert_k[9];
				GetControlPointsWeights( kx, ky, kx1, ky1, vert_k );

				const unsigned int ind= w + z * patch_gird_size[0];

				const m_Vec3 pos=
					base_vertices[0] * vert_k[0] +
					base_vertices[1] * vert_k[1] +
					base_vertices[2] * vert_k[2] +
					base_vertices[3] * vert_k[3] +
					base_vertices[4] * vert_k[4] +
					base_vertices[5] * vert_k[5] +
					base_vertices[6] * vert_k[6] +
					base_vertices[7] * vert_k[7] +
					base_vertices[8] * vert_k[8];
				vert[ind].pos[0]= pos.x;
				vert[ind].pos[1]= pos.y;
				vert[ind].pos[2]= pos.z;

				std::memcpy( vert[ind].tex_maps, tex_maps, 4 );

				const m_Vec2 lightmap_coord=
					base_vertices_lightmap_coords[0] * vert_k[0] +
					base_vertices_lightmap_coords[1] * vert_k[1] +
					base_vertices_lightmap_coords[2] * vert_k[2] +
					base_vertices_lightmap_coords[3] * vert_k[3] +
					base_vertices_lightmap_coords[4] * vert_k[4] +
					base_vertices_lightmap_coords[5] * vert_k[5] +
					base_vertices_lightmap_coords[6] * vert_k[6] +
					base_vertices_lightmap_coords[7] * vert_k[7] +
					base_vertices_lightmap_coords[8] * vert_k[8];
				vert[ind].lightmap_coord[0]= lightmap_coord.x;
				vert[ind].lightmap_coord[1]= lightmap_coord.y;

				const m_Vec2 tex_coord=
					base_vertices_texture_coords[0] * vert_k[0] +
					base_vertices_texture_coords[1] * vert_k[1] +
					base_vertices_texture_coords[2] * vert_k[2] +
					base_vertices_texture_coords[3] * vert_k[3] +
					base_vertices_texture_coords[4] * vert_k[4] +
					base_vertices_texture_coords[5] * vert_k[5] +
					base_vertices_texture_coords[6] * vert_k[6] +
					base_vertices_texture_coords[7] * vert_k[7] +
					base_vertices_texture_coords[8] * vert_k[8];
				vert[ind].tex_coord[0]= tex_coord.x;
				vert[ind].tex_coord[1]= tex_coord.y;

				m_Vec3 normal= GenCurveNormal( base_vertices, kx, ky, kx1, ky1 );
				const float normal_len= normal.Length();
				if( normal_len > g_normal_len_eps )
					normal/= normal_len;
				normals[ind].xyz[0]= (char)(normal.x * 127.0f);
				normals[ind].xyz[1]= (char)(normal.y * 127.0f);
				normals[ind].xyz[2]= (char)(normal.z * 127.0f);

			}// for patch subdivisions x
		}// for patch subdivisions y

		const unsigned int index_count= subdivisions[0] * subdivisions[1] * 6;
		out_indeces.resize( out_indeces.size() + index_count );
		unsigned int* indeces= out_indeces.data() + out_indeces.size() - index_count;

		for( unsigned int z= 0; z< subdivisions[1]; z++ )
		for( unsigned int w= 0; w< subdivisions[0]; w++ )
		{
			indeces[0]= first_out_vertex_index + w +    z    * patch_gird_size[0];
			indeces[1]= first_out_vertex_index + w+1 +  z    * patch_gird_size[0];
			indeces[2]= first_out_vertex_index + w+1 + (z+1) * patch_gird_size[0];
			indeces[3]= first_out_vertex_index + w +    z    * patch_gird_size[0];
			indeces[4]= first_out_vertex_index + w+1 + (z+1) * patch_gird_size[0];
			indeces[5]= first_out_vertex_index + w   + (z+1) * patch_gird_size[0];
			indeces+= 6;
		}
	} // for control vertices (x, y)
}

void GenCurvesMeshes(
	const plb_CurvedSurfaces& curves, const plb_Vertices& curves_vertices,
	plb_Vertices& out_vertices, std::vector<unsigned int>& out_indeces, plb_Normals& out_normals )
{
	for( const plb_CurvedSurface& curve : curves )
	{
		GenCurveMesh( curve, curves_vertices, out_vertices, out_indeces, out_normals );
	}
}

void CalculateCurveCoordinatesForLightTexels(
	const plb_CurvedSurface& curve,
	const m_Vec2& lightmap_coord_scaler, const m_Vec2& lightmap_coord_shift,
	const unsigned int* lightmap_size,
	const plb_Vertices& vertices,
	PositionAndNormal* out_coordinates )
{
	// Fill normal with zero - indicates no geometry
	for( unsigned int i= 0; i < lightmap_size[0] * lightmap_size[1]; i++ )
		out_coordinates[i].normal= m_Vec3( 0.0f, 0.0f, 0.0f );

	typedef plb_Rasterizer<PositionAndNormal> Rasterizer;

	Rasterizer::Buffer buffer;
	buffer.data= out_coordinates;
	buffer.size[0]= lightmap_size[0];
	buffer.size[1]= lightmap_size[1];

	Rasterizer rasterizer( buffer );

	for( unsigned int y= 0; y< curve.grid_size[1]-1; y+=2 )
	for( unsigned int x= 0; x< curve.grid_size[0]-1; x+=2 ) // for curve patches
	{
		const m_Vec3 base_vertices[9]=
		{
			m_Vec3( vertices[ curve.first_vertex_number + x   +  y    * curve.grid_size[0] ].pos ),
			m_Vec3( vertices[ curve.first_vertex_number + x+1 +  y    * curve.grid_size[0] ].pos ),
			m_Vec3( vertices[ curve.first_vertex_number + x+2 +  y    * curve.grid_size[0] ].pos ),
			m_Vec3( vertices[ curve.first_vertex_number + x   + (y+1) * curve.grid_size[0] ].pos ),
			m_Vec3( vertices[ curve.first_vertex_number + x+1 + (y+1) * curve.grid_size[0] ].pos ),
			m_Vec3( vertices[ curve.first_vertex_number + x+2 + (y+1) * curve.grid_size[0] ].pos ),
			m_Vec3( vertices[ curve.first_vertex_number + x   + (y+2) * curve.grid_size[0] ].pos ),
			m_Vec3( vertices[ curve.first_vertex_number + x+1 + (y+2) * curve.grid_size[0] ].pos ),
			m_Vec3( vertices[ curve.first_vertex_number + x+2 + (y+2) * curve.grid_size[0] ].pos ),
		};
		m_Vec2 base_vertices_lightmap_coords[9]=
		{
			m_Vec2( vertices[ curve.first_vertex_number + x   +  y    * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( vertices[ curve.first_vertex_number + x+1 +  y    * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( vertices[ curve.first_vertex_number + x+2 +  y    * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( vertices[ curve.first_vertex_number + x   + (y+1) * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( vertices[ curve.first_vertex_number + x+1 + (y+1) * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( vertices[ curve.first_vertex_number + x+2 + (y+1) * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( vertices[ curve.first_vertex_number + x   + (y+2) * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( vertices[ curve.first_vertex_number + x+1 + (y+2) * curve.grid_size[0] ].lightmap_coord ),
			m_Vec2( vertices[ curve.first_vertex_number + x+2 + (y+2) * curve.grid_size[0] ].lightmap_coord ),
		};
		for( unsigned int i= 0; i < 9; i++ )
			base_vertices_lightmap_coords[i]=
				m_Vec2(
					base_vertices_lightmap_coords[i].x * lightmap_coord_scaler.x,
					base_vertices_lightmap_coords[i].y * lightmap_coord_scaler.y )
					+ lightmap_coord_shift;

		const auto get_lightmap_coord_and_pos=
			[&]( const float kx, const float ky, m_Vec2& out_lightmap_coord, m_Vec3& out_pos )
			{
				const float kx1= 1.0f - kx;
				const float ky1= 1.0f - ky;
				float vert_k[9];
				GetControlPointsWeights( kx, ky, kx1, ky1, vert_k );

				out_lightmap_coord= m_Vec2( 0.0f, 0.0f );
				out_pos= m_Vec3( 0.0f, 0.0f, 0.0f );

				for( unsigned int i= 0; i < 9; i++ )
				{
					out_lightmap_coord+= base_vertices_lightmap_coords[i] * vert_k[i];
					out_pos+= base_vertices[i] * vert_k[i];
				}
			};

		unsigned int subdivisions[2];
		GetPatchSubdivisions( base_vertices, g_max_angle_rad, subdivisions );

		const float step_x = 1.0f / float(subdivisions[0]);
		const float step_y = 1.0f / float(subdivisions[1]);
		// Draw result triangles
		for( unsigned int w= 0; w< subdivisions[1]; w++ )
		for( unsigned int z= 0; z< subdivisions[0]; z++ )
		{
			const float kx= float(z) * step_x;
			const float ky= float(w) * step_y;
			const float kx1= 1.0f - kx;
			const float ky1= 1.0f - ky;
			m_Vec2 vert[4];
			PositionAndNormal attrib[4];

			get_lightmap_coord_and_pos( kx, ky, vert[1], attrib[1].pos );
			attrib[1].normal= GenCurveNormal( base_vertices, kx, ky, kx1, ky1 );

			get_lightmap_coord_and_pos( kx + step_x, ky, vert[3], attrib[3].pos );
			attrib[3].normal= GenCurveNormal( base_vertices, kx + step_x, ky, kx1 - step_x, ky1 );

			get_lightmap_coord_and_pos( kx, ky + step_y, vert[0], attrib[0].pos );
			attrib[0].normal= GenCurveNormal( base_vertices, kx, ky + step_y, kx1, ky1 - step_y );

			get_lightmap_coord_and_pos( kx + step_x, ky + step_y, vert[2], attrib[2].pos );
			attrib[2].normal= GenCurveNormal( base_vertices, kx + step_x, ky + step_y, kx1 - step_x, ky1 - step_y );

			rasterizer.DrawTriangle( vert, attrib );
			rasterizer.DrawTriangle( vert + 1, attrib + 1 );
		}
	} // for curve patches

	// Normalize normal after rasterization
	for( unsigned int i= 0; i < lightmap_size[0] * lightmap_size[1]; i++ )
	{
		const float normal_length= out_coordinates[i].normal.Length();
		if( normal_length > 0.0f )
			out_coordinates[i].normal/= normal_length;
	}

	// Zalivajem pustyje tocki v svetokarte smežnymi znacenijami poziçii i normali.
	// TODO - vozmožno stoit kak-to ekstrapolirovatj poziçiju?
	for( unsigned int iteration= 0u; iteration < 16u; iteration++ )
	{
		for( unsigned int y= 0u; y < lightmap_size[1]; y++ )
		for( unsigned int x= 0u; x < lightmap_size[0]; x++ )
		{
			auto& texel= out_coordinates[ x + y * lightmap_size[0] ];
			if( texel.normal.SquareLength() >= g_normal_len_eps )
				continue;

			unsigned int count= 0u;
			PositionAndNormal avg;
			avg.pos= m_Vec3( 0.0f, 0.0f, 0.0f );
			avg.normal= m_Vec3( 0.0f, 0.0f, 0.0f );
			static const int c_deltas[4][2]= { { -1, 0 }, { +1, 0 }, { 0, -1 }, { 0, +1 } };
			for( unsigned int d= 0; d < 4u; ++d )
			{
				const int x_clamped= std::max( 0, std::min( int(x) + c_deltas[d][0], int(lightmap_size[0]) - 1 ) );
				const int y_clamped= std::max( 0, std::min( int(y) + c_deltas[d][1], int(lightmap_size[1]) - 1 ) );

				const auto& near= out_coordinates[ x_clamped + y_clamped * lightmap_size[0] ];
				if( near.normal.SquareLength() < g_normal_len_eps )
					continue;

				avg.pos+= near.pos;
				avg.normal+= near.normal;
				++count;
			}
			if( count > 0u )
			{
				texel.pos= avg.pos / static_cast<float>(count);
				texel.normal= avg.normal / static_cast<float>(count);
				texel.normal.Normalize();
			}
		} // for yx
	} // for iterations
}
