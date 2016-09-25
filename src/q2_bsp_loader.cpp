#include <cmath>
#include <cstring>
#include <vector>

#include <vec.hpp>

#include "formats.hpp"
#include "loaders_common.hpp"
#include "math_utils.hpp"

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

static void GetOriginForModel( unsigned int model_number, float* origin )
{
	origin[0]= origin[1]= origin[2]= 0.0f;

	char model_name[16];
	std::snprintf( model_name, sizeof(model_name), "*%d", model_number );

	for( const entity_t* entity= entities; entity < entities + num_entities; entity++ )
	{
		if( std::strcmp(
			ValueForKey( const_cast<entity_t*>(entity), "model" ),
			model_name ) == 0 )
		{
			GetVectorForKey( const_cast<entity_t*>(entity), "origin", origin );
			return;
		}
	}
}

static void LoadPolygons(
	plb_Vertices& out_vertices,
	plb_Polygons& out_polygons,
	std::vector<unsigned int>& out_indeces,
	plb_Polygons& out_sky_polygons,
	std::vector<unsigned int>& out_sky_indeces )
{
	for( const dmodel_t* model= dmodels; model < dmodels + nummodels; model++ )
	{
		const unsigned int model_number= model - dmodels;

		float origin[3];
		GetOriginForModel( model_number, origin );

		for(
			const dface_t* face= dfaces + model->firstface;
			face < dfaces + model->firstface + model->numfaces;
			face++ )
		{
			const texinfo_t& tex= texinfo[ face->texinfo ];

			const bool is_sky= ( tex.flags & SURF_SKY ) != 0;

			if( !is_sky && ( tex.flags & (SURF_NODRAW | SURF_SKIP) ) != 0 )
				continue;

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
			// Lightmap basis is projection of texture basis to surface plane.
			for( unsigned int j= 0; j < 2; j++ )
			{
				const m_Vec3 scaled_tex_basis= m_Vec3( tex.vecs[j] ) / 16.0f;

				const m_Vec3 projected=
					plbProjectVectorToPlane(
						scaled_tex_basis,
						m_Vec3( poly.normal ) );

				const float square_length= projected.SquareLength();
				poly.lightmap_basis[j][0]= projected.x / square_length;
				poly.lightmap_basis[j][1]= projected.y / square_length;
				poly.lightmap_basis[j][2]= projected.z / square_length;
			}

			m_Mat3 inverse_lightmap_basis;
			plbGetInvLightmapBasisMatrix(
				m_Vec3( poly.lightmap_basis[0] ),
				m_Vec3( poly.lightmap_basis[1] ),
				inverse_lightmap_basis );

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

				out_vertex.pos[0]= in_vertex.point[0] + origin[0];
				out_vertex.pos[1]= in_vertex.point[1] + origin[1];
				out_vertex.pos[2]= in_vertex.point[2] + origin[2];

				for( unsigned int j= 0; j < 2; j++ )
				{
					out_vertex.tex_coord[j]=
						tex.vecs[j][0] * in_vertex.point[0] +
						tex.vecs[j][1] * in_vertex.point[1] +
						tex.vecs[j][2] * in_vertex.point[2] +
						tex.vecs[j][3];
				}

				m_Vec2 lm= ( m_Vec3(out_vertex.pos) * inverse_lightmap_basis ).xy();
				for( unsigned int j= 0; j < 2; j++ )
					if( lm.ToArr()[j] < lm_min[j] )
						lm_min[j]= lm.ToArr()[j];
			}

			const m_Vec3 center_projected=
					plbProjectPointToPlane(
						m_Vec3( 0.0f, 0.0f, 0.0f ),
						m_Vec3( out_vertices[ first_vertex ].pos ),
						m_Vec3( poly.normal ) );

			const m_Vec3 lightmap_pos=
				center_projected +
				m_Vec3(poly.lightmap_basis[0]) * lm_min[0] +
				m_Vec3(poly.lightmap_basis[1]) * lm_min[1];

			poly.lightmap_pos[0]= lightmap_pos.x;
			poly.lightmap_pos[1]= lightmap_pos.y;
			poly.lightmap_pos[2]= lightmap_pos.z;

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

			poly.flags= 0;

			if( ( tex.flags & SURF_WARP ) != 0 ) // Quake-II warp surfaces has no lightmap.
				poly.flags|= plb_SurfaceFlags::NoLightmap;

			if( model_number != 0u )
				poly.flags|= plb_SurfaceFlags::NoShadow;

		} // for faces
	} // for models
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
		const bool is_point_light= std::strcmp( classname, "light" ) == 0;
		bool is_cone_light= std::strcmp( classname, "light_spot" ) == 0;
		if( is_point_light || is_cone_light )
		{
			plb_ConeLight light;
			light.intensity = 0.0f;
			light.color[0]= light.color[1]= light.color[2]= 255;

			light.direction[0]= 0.0f; light.direction[1]= 0.0f;
			light.direction[2]= -1.0f;

			const float c_max_angle_sin= 0.8f;
			const float c_min_dist_to_target= 64.0f;
			const float c_min_target_radius= 64.0f;

			GetVectorForKey( const_cast<entity_t*>(&ent), "origin", light.pos );

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

						float target_pos[3];
						GetVectorForKey( const_cast<entity_t*>(target), "origin", target_pos );

						const float target_radius=
							std::min( FloatForKey( const_cast<entity_t*>(&ent), "radius" ), c_min_target_radius );

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
					}
				}
				else if( std::strcmp( epair->key, "_cone" ) == 0 )
				{
					light.angle= std::atof( epair->value ) * plb_Constants::to_rad;
					light.angle=
						std::max(
							10.0f * plb_Constants::to_rad,
							std::min( light.angle, std::asin(c_max_angle_sin) ) );
				}
				else if( std::strcmp( epair->key, "angle" ) == 0 )
				{
					const float angle= std::atof( epair->value );
					if( angle == -1 ) // UP
					{
						light.direction[0]= 0.0f; light.direction[1]= 0.0f;
						light.direction[2]= 1.0f;
					}
					else if( angle == -2 ) // DOWN
					{
						light.direction[0]= 0.0f; light.direction[1]= 0.0f;
						light.direction[2]= -1.0f;
					}
					else
					{
						light.direction[0] = std::cos( angle * plb_Constants::to_rad );
						light.direction[1] = std::sin( angle * plb_Constants::to_rad );
						light.direction[2]= 0.0f;
					}
				}

				epair= epair->next;
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

	plbTransformCoordinatesFromQuakeSystem(
		level_data.polygons,
		level_data.vertices,
		level_data.polygons_indeces,
		level_data.sky_polygons_indeces );

	GetBSPLights( level_data.point_lights, level_data.cone_lights );
}
