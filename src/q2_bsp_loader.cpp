#include <cmath>
#include <cstring>
#include <vector>

#include <vec.hpp>

#include "math_utils.hpp"

#include "q3_bsp_loader.hpp"

extern "C"
{

#define false Q2_false
#define true Q2_true

#include <common/cmdlib.h>
#include <common/mathlib.h>
#include <common/bspfile.h>

#undef true
#undef false

}

#define Q_UNITS_IN_METER 64.0f
#define INV_Q_UNITS_IN_METER 0.015625f

#define Q_LIGHT_UNITS_INV_SCALER (1.0f/64.0f)

static void LoadMaterials(
	plb_Materials& out_materials,
	plb_ImageInfos& out_textures )
{
	const auto get_texture=
		[&]( const char* file_name ) -> unsigned int
		{
			for( const plb_ImageInfo& img : out_textures )
			{
				if( Q_strcasecmp( (char*)img.file_name.c_str(), (char*) file_name ) == 0 )
					return &img - out_textures.data();
			}

			out_textures.emplace_back();
			out_textures.back().file_name= file_name;

			return out_textures.size() - 1u;
		};

	for( const texinfo_t* tex= texinfo; tex < texinfo + numtexinfo; tex++ )
	{
		out_materials.emplace_back();
		plb_Material& material= out_materials.back();

		material.albedo_texture_number=
		material.light_texture_number= get_texture( tex->texture );

		if( ( tex->flags & SURF_LIGHT ) != 0 )
			material.luminosity= float(tex->value) * Q_LIGHT_UNITS_INV_SCALER;
	}
}

static void LoadPolygons(
	plb_Vertices& out_vertices,
	plb_Polygons& out_polygons,
	std::vector<unsigned int>& out_indeces,
	plb_Polygons& out_sky_polygons,
	std::vector<unsigned int>& out_sky_indeces )
{
	for( const dface_t* face= dfaces; face < dfaces + numfaces; face++ )
	{
		const texinfo_t& tex= texinfo[ face->texinfo ];

		if( ( tex.flags & (SURF_NODRAW) ) != 0 )
			continue;

		const bool is_sky= ( tex.flags & SURF_SKY ) != 0;

		plb_Polygons& current_polygons= is_sky ? out_sky_polygons : out_polygons;
		std::vector<unsigned int>& current_indeces= is_sky ? out_sky_indeces : out_indeces;

		current_polygons.emplace_back();
		plb_Polygon& poly= current_polygons.back();

		const float side= face->side == 0 ? 1.0f : -1.0f;
		poly.normal[0]= dplanes[ face->planenum ].normal[0] * side;
		poly.normal[1]= dplanes[ face->planenum ].normal[1] * side;
		poly.normal[2]= dplanes[ face->planenum ].normal[2] * side;

		const float normal_length= m_Vec3( poly.normal ).Length();
		poly.normal[0]/= normal_length;
		poly.normal[1]/= normal_length;
		poly.normal[2]/= normal_length;

		// Calculate lightmap basis.
		// Lightmap basis is projection of texture basis to surface plane
		for( unsigned int j= 0; j < 2; j++ )
		{
			poly.lightmap_basis[j][0]= tex.vecs[j][0] / 16.0f;
			poly.lightmap_basis[j][1]= tex.vecs[j][1] / 16.0f;
			poly.lightmap_basis[j][2]= tex.vecs[j][2] / 16.0f;

			const float square_length= m_Vec3( poly.lightmap_basis[j] ).SquareLength();
			poly.lightmap_basis[j][0]/= square_length;
			poly.lightmap_basis[j][1]/= square_length;
			poly.lightmap_basis[j][2]/= square_length;

			const m_Vec3 projected=
				plbProjectPointToPlane(
					m_Vec3( poly.lightmap_basis[j] ),
					m_Vec3( 0.0f, 0.0f, 0.0f ),
					m_Vec3( poly.normal ) );
			poly.lightmap_basis[j][0]= projected.x;
			poly.lightmap_basis[j][1]= projected.y;
			poly.lightmap_basis[j][2]= projected.z;
		}


		float lm_min[2]= { 1e32f, 1e32f };

		const unsigned int first_vertex= out_vertices.size();
		out_vertices.resize( out_vertices.size() + face->numedges );
		for( unsigned int i= 0; i < (unsigned int)face->numedges; i++ )
		{
			const int e= dsurfedges[ face->firstedge + i ];

			const dvertex_t& in_vertex=
				e >= 0
					? dvertexes[ dedges[+e].v[0] ]
					: dvertexes[ dedges[-e].v[1] ];

			plb_Vertex& out_vertex= out_vertices[ first_vertex +  i ];

			out_vertex.pos[0]= in_vertex.point[0];
			out_vertex.pos[1]= in_vertex.point[1];
			out_vertex.pos[2]= in_vertex.point[2];

			for( unsigned int j= 0; j < 2; j++ )
			{
				out_vertex.tex_coord[j]=
					tex.vecs[j][0] * in_vertex.point[0] +
					tex.vecs[j][1] * in_vertex.point[1] +
					tex.vecs[j][2] * in_vertex.point[2] +
					tex.vecs[j][3];
			}

			for( unsigned int j= 0; j < 2; j++ )
			{
				const m_Vec3 lm_basis_vec( poly.lightmap_basis[j] );
				const float lm= lm_basis_vec / lm_basis_vec.SquareLength() * m_Vec3(in_vertex.point);
				if( lm < lm_min[j] ) lm_min[j]= lm;
			}
		}

		// Calculate lightmap pos
		for( unsigned int j= 0; j < 3; j++ )
			poly.lightmap_pos[j]= poly.lightmap_basis[0][j] * lm_min[0] + poly.lightmap_basis[1][j] * lm_min[1];

		// Project lightmap pos
		const m_Vec3 lightmap_pos_projected=
			plbProjectPointToPlane(
				m_Vec3( poly.lightmap_pos ),
				m_Vec3( out_vertices[ first_vertex ].pos ),
				m_Vec3( poly.normal ) );
		poly.lightmap_pos[0]= lightmap_pos_projected.x;
		poly.lightmap_pos[1]= lightmap_pos_projected.y;
		poly.lightmap_pos[2]= lightmap_pos_projected.z;

		poly.material_id= face->texinfo;
		poly.first_vertex_number= first_vertex;
		poly.vertex_count= static_cast<unsigned int>( face->numedges );

		const unsigned int first_index= current_indeces.size();
		const unsigned int triangle_count= static_cast<unsigned int>( face->numedges - 2 );
		current_indeces.resize( current_indeces.size() + triangle_count * 3u );

		for( unsigned int i= 0; i < triangle_count; i++ )
		{
			current_indeces[ first_index + i*3     ]= first_vertex;
			current_indeces[ first_index + i*3 + 1 ]= first_vertex + i + 1;
			current_indeces[ first_index + i*3 + 2 ]= first_vertex + i + 2;
		}

		poly.first_index= first_index;
		poly.index_count= triangle_count * 3;

	} // for faces
}

static void TransformPolygonsCoordinates(
	plb_Polygons& polygons,
	plb_Vertices& vertices,
	std::vector<unsigned int>& indeces,
	std::vector<unsigned int>& sky_indeces )
{
	// transform vertices
	for( plb_Vertex& v : vertices )
	{
		std::swap( v.pos[1], v.pos[2] );
		v.pos[0]*= INV_Q_UNITS_IN_METER;
		v.pos[1]*= INV_Q_UNITS_IN_METER;
		v.pos[2]*= INV_Q_UNITS_IN_METER;
	}

	// transform polygons
	for( plb_Polygon& p : polygons )
	{
		std::swap(p.normal[1], p.normal[2]);

		for( unsigned int i= 0 ;i < 2; i++ )
			std::swap( p.lightmap_basis[i][1], p.lightmap_basis[i][2] );

		std::swap( p.lightmap_pos[1], p.lightmap_pos[2] );

		p.lightmap_basis[0][0]*= INV_Q_UNITS_IN_METER;
		p.lightmap_basis[0][1]*= INV_Q_UNITS_IN_METER;
		p.lightmap_basis[0][2]*= INV_Q_UNITS_IN_METER;
		p.lightmap_basis[1][0]*= INV_Q_UNITS_IN_METER;
		p.lightmap_basis[1][1]*= INV_Q_UNITS_IN_METER;
		p.lightmap_basis[1][2]*= INV_Q_UNITS_IN_METER;
		p.lightmap_pos[0]*= INV_Q_UNITS_IN_METER;
		p.lightmap_pos[1]*= INV_Q_UNITS_IN_METER;
		p.lightmap_pos[2]*= INV_Q_UNITS_IN_METER;
	}

	for( unsigned int i= 0; i < indeces.size(); i+= 3 )
		std::swap( indeces[i], indeces[i+1] );

	for( unsigned int i= 0; i < sky_indeces.size(); i+= 3 )
		std::swap( sky_indeces[i], sky_indeces[i+1] );
}

static void ParseColor( const char* str, unsigned char* out_color )
{
	double rgb[3];

	sscanf( str, "%lf %lf %lf", &rgb[0], &rgb[1], &rgb[2] );
	for( unsigned int i= 0; i < 3; i++ )
	{
		if( rgb[i] > 255.0 ) rgb[i]= 255.0;
		if( rgb[i] < 0.0 ) rgb[i]= 0.0;

		out_color[i]= static_cast<unsigned char>( rgb[i] );
	}
}

static void ParseColorF( const char* str, unsigned char* out_color )
{
	double rgb[3];

	sscanf( str, "%lf %lf %lf", &rgb[0], &rgb[1], &rgb[2] );
	for( unsigned int i= 0; i < 3; i++ )
	{
		rgb[i]*= 255.0;
		if( rgb[i] > 255.0 ) rgb[i]= 255.0;
		if( rgb[i] < 0.0 ) rgb[i]= 0.0;

		out_color[i]= static_cast<unsigned char>( rgb[i] );
	}
}

static const entity_t* FindTarget( const char* key )
{
	for( unsigned int i= 0; i < (unsigned int)num_entities; i++ )
	{
		const entity_t& ent = entities[i];
		const char* const name= ValueForKey( const_cast<entity_t*>(&ent), "targetname" );
		if( std::strcmp( name, key ) == 0 )
			return &ent;
	}

	return nullptr;
}

static void GetBSPLights( plb_PointLights& point_lights, plb_ConeLights& cone_lights )
{
	for( unsigned int i= 0; i < (unsigned int)num_entities; i++ )
	{
		const entity_t& ent = entities[i];
		if( ent.epairs == nullptr ) continue;

		const char* const classname= ValueForKey( const_cast<entity_t*>(&ent), "classname" );
		if( std::strcmp( classname, "light" ) == 0 )
		{
			bool is_cone_light= false;
			plb_ConeLight light;
			light.intensity = 0.0f;
			light.color[0]= light.color[1]= light.color[2]= 255;

			float target_pos[3];
			float target_radius;

			const float c_max_angle_sin= 0.8f;
			const float c_min_dist_to_target= 64.0f;
			const float c_min_target_radius= 64.0f;

			const epair_t* epair= ent.epairs;
			while( epair != nullptr )
			{
				if( std::strcmp( epair->key, "light" ) == 0 )
					light.intensity= std::atof(epair->value);
				else if( std::strcmp( epair->key, "color" ) == 0 )
					ParseColor( epair->value, light.color );
				else if( std::strcmp( epair->key, "_color" ) == 0 )
					ParseColorF( epair->value, light.color );
				else if( std::strcmp( epair->key, "target" ) == 0 )
				{
					if( const entity_t* const target= FindTarget( epair->value ) )
					{
						is_cone_light= true;
						GetVectorForKey( const_cast<entity_t*>(target), "origin", target_pos );

						target_radius= FloatForKey( const_cast<entity_t*>(&ent), "radius" );
						if( target_radius < c_min_target_radius )
							target_radius= c_min_target_radius;
					}
				}

				epair= epair->next;
			}
			GetVectorForKey( const_cast<entity_t*>(&ent), "origin", light.pos );

			if( is_cone_light )
			{
				const m_Vec3 dir= m_Vec3(target_pos) - m_Vec3(light.pos);
				const float len= dir.Length();
				if( len != 0.0f )
				{
					light.angle=
						std::asin(
							std::min(
								target_radius / std::max( len, c_min_dist_to_target ),
								c_max_angle_sin ) );

					for( unsigned int c= 0; c < 3; c++ )
						light.direction[c]= dir.ToArr()[c] / len;
				}
				else // fallback
					is_cone_light= false;
			}

			// Change coord system
			std::swap(light.pos[1], light.pos[2]);
			std::swap(light.direction[1], light.direction[2]);
			for( unsigned int j = 0; j < 3; j++)
				light.pos[j]*= INV_Q_UNITS_IN_METER;

			light.intensity*= Q_LIGHT_UNITS_INV_SCALER;

			if( is_cone_light )
				cone_lights.push_back(light);
			else
				point_lights.push_back(light);
		} // if light

	} // for entities
}

PLB_DLL_FUNC void LoadBsp(
	const char* file_name,
	const plb_Config& config,
	plb_LevelData& level_data )
{
	LoadBSPFile( const_cast<char*>(file_name) );
	ParseEntities();

	LoadMaterials( level_data.materials, level_data.textures );

	LoadPolygons(
		level_data.vertices, level_data.polygons, level_data.polygons_indeces,
		level_data.sky_polygons, level_data.sky_polygons_indeces );

	TransformPolygonsCoordinates(
		level_data.polygons,
		level_data.vertices,
		level_data.polygons_indeces,
		level_data.sky_polygons_indeces );

	GetBSPLights( level_data.point_lights, level_data.cone_lights );
}
