#include <cstring>

#include <vec.hpp>

#include "curves.hpp"

void GetPatchSubdivisions( const m_Vec3* control_vertices, float max_angle_rad, unsigned int* out_subdivisions )
{
	const float eps= 1e-3f;

	float max_angle_x= 0.0f, max_angle_y= 0.0f;
	for( unsigned int i= 0; i< 3; i++ )
	{
		m_Vec3 vec0= control_vertices[i*3+1] - control_vertices[i*3+0];
		m_Vec3 vec1= control_vertices[i*3+2] - control_vertices[i*3+1];
		float l0= vec0.Length();
		float l1= vec1.Length();
		if( l0 > eps && l1 > eps )
		{
			float dot= (vec0 * vec1) / ( l0 * l1 );
			float angle= acosf(dot);
			if( angle > max_angle_x ) max_angle_x= angle;
		}
	}
	for( unsigned int i= 0; i< 3; i++ )
	{
		m_Vec3 vec0= control_vertices[3+i] - control_vertices[i+0];
		m_Vec3 vec1= control_vertices[6+i] - control_vertices[3+i];
		float l0= vec0.Length();
		float l1= vec1.Length();
		if( l0 > eps && l1 > eps )
		{
			float dot= (vec0 * vec1) / ( l0 * l1 );
			float angle= acosf(dot);
			if( angle > max_angle_y ) max_angle_y= angle;
		}
	}

	out_subdivisions[0]= (unsigned int)ceilf( max_angle_x / max_angle_rad );
	if( out_subdivisions[0] < 1 ) out_subdivisions[0]= 1;
	out_subdivisions[1]= (unsigned int)ceilf( max_angle_y / max_angle_rad );
	if( out_subdivisions[1] < 1 ) out_subdivisions[1]= 1;
}

static const float two_div_max_rand = 2.0f / float(RAND_MAX);

static inline m_Vec3 GenCurveNormal( const m_Vec3* base_vertices, float kx, float ky, float kx1, float ky1 )
{
	static const float eps= 1e-5f;
	static const float control_verties_k[]=
	{
		0.0f,0.0f, 0.5f,0.0f, 1.0f,0.0f,
		0.0f,0.5f, 0.5f,0.5f, 1.0f,0.5f,
		0.0f,1.0f, 0.5f,1.0f, 1.0f,1.0f,
		0.33f,0.66f, 0.66f,0.33f,
		0.25f,0.75f, 0.75f,0.25f,
	};

	const float in_k[]= { kx, ky };

	m_Vec3 result(1.0f, 0.0f, 0.0f );

	for( unsigned int i= 0; i< sizeof(control_verties_k) / (2*sizeof(float)); i++ )
	{
		float d_pos_dx_kx[3];
		d_pos_dx_kx[0]= 2.0f * kx - 2.0f;
		d_pos_dx_kx[1]= 2.0f - 4.0f * kx;
		d_pos_dx_kx[2]= 2.0f * kx;
		m_Vec3 d_pos_dx= 
			ky1 *
				( base_vertices[0]*d_pos_dx_kx[0] + base_vertices[1]*d_pos_dx_kx[1] + base_vertices[2]*d_pos_dx_kx[2] ) +
			2.0f * ky * ky1 *
				( base_vertices[3]*d_pos_dx_kx[0] + base_vertices[4]*d_pos_dx_kx[1] + base_vertices[5]*d_pos_dx_kx[2] ) +
			ky * ky * 
				( base_vertices[6]*d_pos_dx_kx[0] + base_vertices[7]*d_pos_dx_kx[1] + base_vertices[8]*d_pos_dx_kx[2] );

		d_pos_dx_kx[0]= kx1 * kx1;
		d_pos_dx_kx[1]= 2.0f * kx * kx1;
		d_pos_dx_kx[2]= kx * kx;
		m_Vec3 d_pos_dy= 
			( 2.0f * ky - 2.0f ) *
				( base_vertices[0]*d_pos_dx_kx[0] + base_vertices[1]*d_pos_dx_kx[1] + base_vertices[2]*d_pos_dx_kx[2] ) +
			( 2.0f - 4.0f * ky ) *
				( base_vertices[3]*d_pos_dx_kx[0] + base_vertices[4]*d_pos_dx_kx[1] + base_vertices[5]*d_pos_dx_kx[2] ) +
			2.0f * ky *
				( base_vertices[6]*d_pos_dx_kx[0] + base_vertices[7]*d_pos_dx_kx[1] + base_vertices[8]*d_pos_dx_kx[2] );

		float l0= d_pos_dx.Length();
		float l1= d_pos_dy.Length();
		if( l0 > eps && l1 > eps )
		{
			result= mVec3Cross( d_pos_dy, d_pos_dx );
			float rl= result.Length();
			if( rl > eps )
			{
				result/= rl;
				break;
			}
		}

		kx= in_k[0] * 0.92f +  control_verties_k[i*2  ] * 0.08f;
		ky= in_k[1] * 0.92f +  control_verties_k[i*2+1] * 0.08f;

		kx1= 1.0f - kx;
		ky1= 1.0f - ky;
	}// for iterations

	return result;
}

void GenCurvesMeshes(
	const plb_CurvedSurfaces& curves, const plb_Vertices& curves_vertices,
	plb_Vertices& out_vertices, std::vector<unsigned int>& out_indeces, plb_Normals& out_normals )
{
	const float max_angle_rad= 8.0f * 3.1415926535f / 180.0f;

	const plb_Vertex* v_p= curves_vertices.data();
	for( const plb_CurvedSurface& curve : curves )
	{
		for( unsigned int y= 0; y< curve.grid_size[1]-1; y+=2 )
			for( unsigned int x= 0; x< curve.grid_size[0]-1; x+=2 ) // for curve patches
			{
				m_Vec3 base_vertices[]=
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
				m_Vec2 base_vertices_lightmap_coords[]=
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
				m_Vec2 base_vertices_texture_coords[]=
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
				unsigned int base_vertex_index= curve.first_vertex_number + x +  y * curve.grid_size[0];
				unsigned char tex_maps[4];
				std::memcpy(tex_maps, v_p[ base_vertex_index ].tex_maps, 4 );

				unsigned int subdivisions[2];
				GetPatchSubdivisions( base_vertices, max_angle_rad, subdivisions );

				unsigned int patch_gird_size[]= { subdivisions[0]+1, subdivisions[1]+1 };
				unsigned int first_out_vertex_index= out_vertices.size();
				out_vertices.resize( out_vertices.size() + patch_gird_size[0] * patch_gird_size[1] );
				plb_Vertex* vert= out_vertices.data() + first_out_vertex_index;

				out_normals.resize( out_normals.size() + patch_gird_size[0] * patch_gird_size[1] );
				plb_Normal* normals= out_normals.data() + out_normals.size() - patch_gird_size[0] * patch_gird_size[1];

				float ky= 0.0f, dky= 1.0f / float(subdivisions[1]);
				for( unsigned int z= 0; z< patch_gird_size[1]; z++, ky+= dky )
				{
					float ky1= 1.0f - ky;
					float kx= 0.0f, dkx= 1.0f / float(subdivisions[0]);
					for( unsigned int w= 0; w< patch_gird_size[0]; w++, kx+= dkx )
					{
						float kx1= 1.0f - kx;
						float vert_k[9];
						vert_k[0]= ky1 * ky1 * kx1 * kx1;;
						vert_k[1]= ky1 * ky1 * kx  * kx1 * 2.0f;
						vert_k[2]= ky1 * ky1 * kx  * kx;
						vert_k[3]= ky  * ky1 * kx1 * kx1 * 2.0f;
						vert_k[4]= ky  * ky1 * kx  * kx1 * 4.0f;
						vert_k[5]= ky  * ky1 * kx  * kx  * 2.0f;
						vert_k[6]= ky  * ky  * kx1 * kx1;
						vert_k[7]= ky  * ky  * kx  * kx1 * 2.0f;
						vert_k[8]= ky  * ky  * kx  * kx;
						m_Vec3 pos=
							base_vertices[0] * vert_k[0] +
							base_vertices[1] * vert_k[1] +
							base_vertices[2] * vert_k[2] +
							base_vertices[3] * vert_k[3] +
							base_vertices[4] * vert_k[4] +
							base_vertices[5] * vert_k[5] +
							base_vertices[6] * vert_k[6] +
							base_vertices[7] * vert_k[7] +
							base_vertices[8] * vert_k[8];
						unsigned int ind= w + z * patch_gird_size[0];
						vert[ind].pos[0]= pos.x;
						vert[ind].pos[1]= pos.y;
						vert[ind].pos[2]= pos.z;
						std::memcpy(vert[ind].tex_maps, tex_maps, 4 );

						m_Vec2 lightmap_coord= 
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
						
						m_Vec2 tex_coord=
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

						/*float d_pos_dx_kx[3];
						d_pos_dx_kx[0]= 2.0f * kx - 2.0f;
						d_pos_dx_kx[1]= 2.0f - 4.0f * kx;
						d_pos_dx_kx[2]= 2.0f * kx;
						m_Vec3 d_pos_dx= 
							ky1 *
								( base_vertices[0]*d_pos_dx_kx[0] + base_vertices[1]*d_pos_dx_kx[1] + base_vertices[2]*d_pos_dx_kx[2] ) +
							2.0f * ky * ky1 *
								( base_vertices[3]*d_pos_dx_kx[0] + base_vertices[4]*d_pos_dx_kx[1] + base_vertices[5]*d_pos_dx_kx[2] ) +
							ky * ky * 
								( base_vertices[6]*d_pos_dx_kx[0] + base_vertices[7]*d_pos_dx_kx[1] + base_vertices[8]*d_pos_dx_kx[2] );

						d_pos_dx_kx[0]= kx1 * kx1;
						d_pos_dx_kx[1]= 2.0f * kx * kx1;
						d_pos_dx_kx[2]= kx * kx;
						m_Vec3 d_pos_dy= 
							( 2.0f * ky - 2.0f ) *
								( base_vertices[0]*d_pos_dx_kx[0] + base_vertices[1]*d_pos_dx_kx[1] + base_vertices[2]*d_pos_dx_kx[2] ) +
							( 2.0f - 4.0f * ky ) *
								( base_vertices[3]*d_pos_dx_kx[0] + base_vertices[4]*d_pos_dx_kx[1] + base_vertices[5]*d_pos_dx_kx[2] ) +
							2.0f * ky *
								( base_vertices[6]*d_pos_dx_kx[0] + base_vertices[7]*d_pos_dx_kx[1] + base_vertices[8]*d_pos_dx_kx[2] );

						m_Vec3 normal= mVec3Cross( d_pos_dy, d_pos_dx );
						normal.Normalize();*/
						m_Vec3 normal= GenCurveNormal( base_vertices, kx, ky, kx1, ky1 );

						normals[ind].xyz[0]= (char)(normal.x * 127.0f);
						normals[ind].xyz[1]= (char)(normal.y * 127.0f);
						normals[ind].xyz[2]= (char)(normal.z * 127.0f);

					}// for patch subdivisions x
				}// for patch subdivisions y

				unsigned int index_count= subdivisions[0] * subdivisions[1] * 6;
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
			}// for control vertices
	}// for curves
}
