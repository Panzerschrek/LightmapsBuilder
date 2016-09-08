#include <cmath>
#include <cstring>
#include <iostream>

#include "formats.hpp"
#include "loaders_common.hpp"

extern "C"
{
#include <common/cmdlib.h>
#include <common/mathlib.h>
#include <common/bspfile.h>
#include <common/scriplib.h>

}

#include <vec.hpp>


// HACK. Use windows-specific function for "listfiles"
#ifdef _WIN32
#include <windows.h>

static std::vector<std::string> ShaderInfoFiles( const std::string& shaders_path )
{
	std::vector<std::string> result;

	WIN32_FIND_DATAA find_data;

	HANDLE handle= FindFirstFileA( ( shaders_path + "*.shader" ).c_str(), &find_data );

	if( handle != INVALID_HANDLE_VALUE )
	{
		do
		{
			if( !( ( find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 ||
				std::strcmp( find_data.cFileName, "."  ) == 0 ||
				std::strcmp( find_data.cFileName, ".." ) == 0 ) )
				result.push_back( find_data.cFileName );

		} while( FindNextFileA( handle, &find_data ) );
	}

	FindClose( handle );

	return result;
}
#endif

struct Q3Shader : public plb_Material
{
	std::string name;
	bool used_in_current_level= false;

	unsigned int number_in_level;
};

typedef std::vector<Q3Shader> Q3Shaders;

struct Q3SkyLight
{
	std::string name;

	float color[3]; // Need to normalize
	float intensity;
	float degrees;
	float elevation;
};

typedef std::vector<Q3SkyLight> Q3SkyLights;

static Q3Shaders LoadShaders( const std::string& shaders_dir, Q3SkyLights& out_sky_lights )
{
	const std::vector<std::string> shader_info_files= ShaderInfoFiles( shaders_dir );

	Q3Shaders shaders;

	for( const std::string& shader_file : shader_info_files )
	{
		LoadScriptFile( ( shaders_dir + shader_file ).c_str() );

		while( GetToken(qtrue) )
		{
			shaders.emplace_back();
			Q3Shader& out_shader= shaders.back();

			out_shader.name= token;
			out_shader.albedo_texture_file_name= out_shader.name;

			MatchToken( "{" );
			while( GetToken(qtrue) )
			{
				unsigned int pass_number= 0;

				if( std::strcmp( token, "}" ) == 0 )
					break;
				else if( Q_stricmp( token, "q3map_sun" ) == 0 )
				{
					out_sky_lights.emplace_back();
					Q3SkyLight& light= out_sky_lights.back();

					light.name= out_shader.name;

					GetToken( qfalse );
					light.color[0]= std::atof( token );
					GetToken( qfalse );
					light.color[1]= std::atof( token );
					GetToken( qfalse );
					light.color[2]= std::atof( token );
					GetToken( qfalse );
					light.intensity= std::atof( token );
					GetToken( qfalse );
					light.degrees= std::atof( token );
					GetToken( qfalse );
					light.elevation= std::atof( token );
				}
				else if( Q_stricmp( token, "q3map_surfacelight" ) == 0 )
				{
					GetToken( qfalse );
					out_shader.luminosity= std::atof( token );
				}
				else if( Q_stricmp( token, "q3map_lightimage" ) == 0 )
				{
					GetToken( qfalse );
					out_shader.light_texture_file_name= token;
				}
				else if( Q_stricmp( token, "surfaceparm" ) == 0 )
				{
					GetToken( qfalse );
					if( Q_stricmp( token, "alphashadow" ) == 0 )
						out_shader.cast_alpha_shadow= true;
				}
				// Passes
				// Search passes with textures, which we can use as albedo
				else if ( std::strcmp( token, "{" ) == 0 )
				{
					pass_number++;

					std::string map;
					bool blend_mul= pass_number == 1; // for first pass act, like we multipy by 1
					bool rgbgen_identity= true;

					while( GetToken(qtrue) )
					{
						if( std::strcmp( token, "}" ) == 0 )
							break;
						// Take last "map" or "clampmap" as albedo texture
						else if( Q_stricmp( token, "map" ) == 0 )
						{
							GetToken( qfalse );
							if( token[0] != '$' ) // Skip $ightmap, $whiteimage, etc.
								map= token;
						}
						else if( Q_stricmp( token, "rgbgen" ) == 0 )
						{
							GetToken( qfalse );
							rgbgen_identity= Q_stricmp( token, "identity" ) == 0;
						}
						else if( Q_stricmp( token, "blendfunc" ) == 0 )
						{
							blend_mul= false;

							GetToken( qfalse );
							if( Q_stricmp( token, "filter" ) == 0 )
								blend_mul= true;
							else if( Q_stricmp( token, "gl_dst_color" ) == 0 )
							{
								GetToken( qfalse );
								blend_mul= Q_stricmp( token, "gl_zero" ) == 0;
							}
							else if( Q_stricmp( token, "gl_zero" ) == 0 )
							{
								GetToken( qfalse );
								blend_mul= Q_stricmp( token, "gl_src_color" ) == 0;
							}
						}

					} // Inside pass

					if( !map.empty() && blend_mul && rgbgen_identity )
						out_shader.albedo_texture_file_name= map;

					continue;
				} // Inside passes

			} // Inside shader
		} // parse file
	} // for shader info files

	return shaders;
}


static void BuildMaterials(
	Q3Shaders& shaders,
	plb_Materials& out_materials, plb_ImageInfos& out_textures,
	std::vector<unsigned short>& shader_num_to_material_index )
{
	// Search level shaders in shaders list. Add new shader, if not found
	for( const dshader_t* dshader= dshaders; dshader < dshaders + numShaders; dshader++ )
	{
		Q3Shader* found_shader= nullptr;

		for( Q3Shader& shader : shaders )
			if( Q_stricmp( shader.name.c_str(), dshader->shader ) == 0 )
			{
				found_shader= &shader;
				break;
			}

		if( found_shader == nullptr )
		{
			shaders.emplace_back();
			found_shader = &shaders.back();
			found_shader->albedo_texture_file_name= dshader->shader;
		}

		found_shader->used_in_current_level= true;
		found_shader->number_in_level= dshader - dshaders;
	}

	// Search-inserter for textures.
	const auto get_image=
	[&out_textures]( const std::string& file_name ) -> size_t
	{
		for( const plb_ImageInfo& image : out_textures )
		{
			if( image.file_name == file_name )
				return &image - out_textures.data();
		}

		out_textures.emplace_back();
		out_textures.back().file_name= file_name;

		return out_textures.size() - 1u;
	};

	shader_num_to_material_index.resize( numShaders );

	// Save to out materials only used shaders.
	// Set textures indeces for materials.
	for( const Q3Shader& shader : shaders )
	{
		if( !shader.used_in_current_level )
			continue;

		shader_num_to_material_index[ shader.number_in_level ]= out_materials.size();

		out_materials.emplace_back( shader );
		plb_Material& material= out_materials.back();

		material.luminosity*= Q_LIGHT_UNITS_INV_SCALER;

		material.albedo_texture_number= get_image( material.albedo_texture_file_name );

		if( !material.light_texture_file_name.empty() )
			material.light_texture_number= get_image( material.light_texture_file_name );
		else
			material.light_texture_number= material.albedo_texture_number;
	}
}

static void BuildDirectionalLights( const Q3SkyLights& sky_lights, plb_DirectionalLights& out_lights )
{
	for( const dshader_t* dshader= dshaders; dshader < dshaders + numShaders; dshader++ )
	{
		for( const Q3SkyLight& light_shader : sky_lights )
		{
			if( Q_stricmp( light_shader.name.c_str(), dshader->shader ) != 0 )
				continue;

			// This light used - add it

			out_lights.emplace_back();
			plb_DirectionalLight& light= out_lights.back();

			const float c_to_rad= 3.1415926535f / 180.0f;
			const float elevation= light_shader.elevation * c_to_rad;
			const float degrees= light_shader.degrees * c_to_rad;

			light.direction[1]= std::sin( elevation );
			light.direction[0]= std::cos( elevation ) * std::cos( degrees );
			light.direction[2]= std::cos( elevation ) * std::sin( degrees );

			light.intensity= light_shader.intensity * Q_LIGHT_UNITS_INV_SCALER;

			const float max_in_color_component=
				std::max(
					std::max( light_shader.color[0], light_shader.color[1] ),
					light_shader.color[2] );

			for( unsigned int j= 0; j < 3; j++ )
			{
				int c= static_cast<int>( 255.0f * light_shader.color[j] / max_in_color_component );
				if( c < 0 ) c= 0;
				if( c > 255 ) c= 255;
				light.color[j]= c;
			}

			break;

		} // for loaded sky shaders
	} // for level shaders
}

static void LoadVertices( std::vector<plb_Vertex>& out_vertices )
{
	out_vertices.reserve( numDrawVerts );
	for( const drawVert_t* in_vertex= drawVerts; in_vertex < drawVerts + numDrawVerts; in_vertex++ )
	{
		out_vertices.emplace_back();
		plb_Vertex& out_vertex= out_vertices.back();

		out_vertex.pos[0]= in_vertex->xyz[0];
		out_vertex.pos[1]= in_vertex->xyz[1];
		out_vertex.pos[2]= in_vertex->xyz[2];
		out_vertex.tex_coord[0]= in_vertex->st[0];
		out_vertex.tex_coord[1]= in_vertex->st[1];

		out_vertex.lightmap_coord[0]= in_vertex->lightmap[0];
		out_vertex.lightmap_coord[1]= in_vertex->lightmap[1];
	}
}

static void BuildPolygons(
	const std::vector<unsigned short>& shader_num_to_material_index,
	std::vector<plb_Polygon>& out_polygons, std::vector<plb_Polygon>& out_sky_polygons,
	std::vector<unsigned int>& out_indeces, std::vector<unsigned int>& out_sky_indeces,
	std::vector<plb_CurvedSurface>& out_curves, std::vector<plb_Vertex>& out_curves_vertices )
{
	out_polygons.reserve( numDrawSurfaces );

	for( const dsurface_t* p= drawSurfaces; p < drawSurfaces + numDrawSurfaces; p++ )
	{
		if( p->surfaceType == 1 // polygon, no model or patch
			&& (dshaders[p->shaderNum].surfaceFlags & (SURF_NODRAW) ) == 0
			&& (dshaders[p->shaderNum].contentFlags & (/*CONTENTS_LAVA|*/CONTENTS_SLIME|CONTENTS_FOG) ) == 0
			)
		{
			bool is_sky= (dshaders[p->shaderNum].surfaceFlags & SURF_SKY) != 0;
			std::vector<unsigned int>& indeces_dst= is_sky ? out_sky_indeces : out_indeces;

			const int index_offset= indeces_dst.size();
			indeces_dst.resize( index_offset + p->numIndexes );
			for( int i= 0; i < p->numIndexes; i++ )
				indeces_dst[i + index_offset]= drawIndexes[i + p->firstIndex] + p->firstVert;

			plb_Polygon polygon;

			polygon.first_vertex_number= p->firstVert;
			polygon.vertex_count= p->numVerts;

			polygon.first_index= index_offset;
			polygon.index_count= p->numIndexes;

			polygon.material_id= shader_num_to_material_index[ p->shaderNum ];

			const float normal_length= m_Vec3( p->lightmapVecs[2] ).Length();
			polygon.normal[0]= p->lightmapVecs[2][0] / normal_length;
			polygon.normal[1]= p->lightmapVecs[2][1] / normal_length;
			polygon.normal[2]= p->lightmapVecs[2][2] / normal_length;

			polygon.lightmap_basis[0][0]= p->lightmapVecs[0][0];
			polygon.lightmap_basis[0][1]= p->lightmapVecs[0][1];
			polygon.lightmap_basis[0][2]= p->lightmapVecs[0][2];
			polygon.lightmap_basis[1][0]= p->lightmapVecs[1][0];
			polygon.lightmap_basis[1][1]= p->lightmapVecs[1][1];
			polygon.lightmap_basis[1][2]= p->lightmapVecs[1][2];
			polygon.lightmap_pos[0]= p->lightmapOrigin[0];
			polygon.lightmap_pos[1]= p->lightmapOrigin[1];
			polygon.lightmap_pos[2]= p->lightmapOrigin[2];

			(is_sky ? out_sky_polygons : out_polygons).push_back(polygon);
		}// if normal polygon
		else if( p->surfaceType == 2 )// curve 
		{
			plb_CurvedSurface surf;
			surf.grid_size[0]= p->patchWidth;
			surf.grid_size[1]= p->patchHeight;
			surf.first_vertex_number= out_curves_vertices.size();

			surf.material_id= shader_num_to_material_index[ p->shaderNum ];

			surf.bb_min[0]= p->lightmapVecs[0][0];
			surf.bb_min[1]= p->lightmapVecs[0][1];
			surf.bb_min[2]= p->lightmapVecs[0][2];
			surf.bb_max[0]= p->lightmapVecs[1][0];
			surf.bb_max[1]= p->lightmapVecs[1][1];
			surf.bb_max[2]= p->lightmapVecs[1][2];

			surf.lightmap_data.size[0]= p->lightmapWidth;
			if( surf.lightmap_data.size[0] < 2 )
				surf.lightmap_data.size[0]= 2;
			surf.lightmap_data.size[1]= p->lightmapHeight;
			if( surf.lightmap_data.size[1] < 2 ) 
				surf.lightmap_data.size[1]= 2;

			for( unsigned int v= 0; v< (unsigned int)p->numVerts; v++ )
			{
				unsigned int v_ind= v+p->firstVert;
				plb_Vertex vert;

				vert.pos[0]= drawVerts[v_ind].xyz[0];
				vert.pos[1]= drawVerts[v_ind].xyz[1];
				vert.pos[2]= drawVerts[v_ind].xyz[2];
				vert.tex_coord[0]= drawVerts[v_ind].st[0];
				vert.tex_coord[1]= drawVerts[v_ind].st[1];
				vert.lightmap_coord[0]= drawVerts[v_ind].lightmap[0];
				vert.lightmap_coord[1]= drawVerts[v_ind].lightmap[1];

				out_curves_vertices.push_back(vert);
			}// for patch control vertices

			out_curves.push_back(surf);
		}// of curve
	}// for polygons
}

static void TransformCurves(
	plb_CurvedSurfaces& curves,
	plb_Vertices& curves_vertices )
{
	for( plb_Vertex& v : curves_vertices )
	{
		std::swap(v.pos[1], v.pos[2]);
		v.pos[0]*= INV_Q_UNITS_IN_METER;
		v.pos[1]*= INV_Q_UNITS_IN_METER;
		v.pos[2]*= INV_Q_UNITS_IN_METER;
	}

	for( plb_CurvedSurface& curve : curves )
	{
		std::swap(curve.bb_min[1], curve.bb_min[2]);
		std::swap(curve.bb_max[1], curve.bb_max[2]);

		for( unsigned int i= 0; i < 3; i++ )
		{
			curve.bb_min[i]*= INV_Q_UNITS_IN_METER;
			curve.bb_max[i]*= INV_Q_UNITS_IN_METER;
		}
	}
}

static void GenCurvesNormalizedLightmapCoords(
	plb_CurvedSurfaces& curves,
	plb_Vertices& curves_vertices )
{
	const float infinity= 1e16f;
	const float eps= 1e-3f;

	for( plb_CurvedSurface& curve : curves )
	{
		float uv_min[]= {  infinity,  infinity };
		float uv_max[]= { -infinity, -infinity };
		for( unsigned int v= 0; v< curve.grid_size[0] * curve.grid_size[1]; v++ )
		{
			const plb_Vertex& vert= curves_vertices[ curve.first_vertex_number + v ];
			if( vert.lightmap_coord[0] > uv_max[0] ) uv_max[0]= vert.lightmap_coord[0];
			else if( vert.lightmap_coord[0] < uv_min[0] ) uv_min[0]= vert.lightmap_coord[0];
			if( vert.lightmap_coord[1] > uv_max[1] ) uv_max[1]= vert.lightmap_coord[1];
			else if( vert.lightmap_coord[1] < uv_min[1] ) uv_min[1]= vert.lightmap_coord[1];
		}
		float inv_size[]= { uv_max[0] - uv_min[0], uv_max[1] - uv_min[1] };

		if( inv_size[0] < eps || inv_size[1] < eps )
		{
			for( unsigned int v= 0; v< curve.grid_size[0] * curve.grid_size[1]; v++ )
			{
				plb_Vertex& vert= curves_vertices[ curve.first_vertex_number + v ];
				vert.lightmap_coord[0]= 0.0f;
				vert.lightmap_coord[1]= 0.0f;
			}
		}
		else
		{
			inv_size[0]= 1.0f / inv_size[0];
			inv_size[1]= 1.0f / inv_size[1];

			for( unsigned int v= 0; v< curve.grid_size[0] * curve.grid_size[1]; v++ )
			{
				plb_Vertex& vert= curves_vertices[ curve.first_vertex_number + v ];
				vert.lightmap_coord[0]= (vert.lightmap_coord[0]- uv_min[0]) * inv_size[0];
				vert.lightmap_coord[1]= (vert.lightmap_coord[1]- uv_min[1]) * inv_size[1];
			}
		}
	}// for curves
}

inline static bool IsCharLexemSeparator(char c)
{
	return c == '\n' || c == ' ' || c == '\t';
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
		const char* const name= ValueForKey( &ent, "targetname" );
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

		const char* const classname= ValueForKey( &ent, "classname" );
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
						GetVectorForKey( target, "origin", target_pos );

						target_radius= FloatForKey( &ent, "radius" );
						if( target_radius < c_min_target_radius )
							target_radius= c_min_target_radius;
					}
				}

				epair= epair->next;
			}
			GetVectorForKey( &ent, "origin", light.pos );

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

			if( m_Vec3(light.pos).SquareLength() < 0.1f )
			{
				std::cout << "light at zeto" ;
			}

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
	LoadBSPFile( file_name );

	// std::cout << (file_data + header->lumps[ LUMP_ENTITIES ].fileofs );

	Q3SkyLights sky_lights;
	Q3Shaders shaders= LoadShaders( config.textures_path + "shaders/", sky_lights );

	std::vector<unsigned short> shader_num_to_material_index;
	BuildMaterials( shaders, level_data.materials, level_data.textures, shader_num_to_material_index );

	BuildDirectionalLights( sky_lights, level_data.directional_lights );
	if( level_data.directional_lights.size() > 1 )
	{
		std::cout << "Warning, more, than one directional light per level not supported" << std::endl;
		level_data.directional_lights.resize( 1 );
	}

	LoadVertices( level_data.vertices );

	BuildPolygons(
		shader_num_to_material_index,
		level_data.polygons, level_data.sky_polygons,
		level_data.polygons_indeces, level_data.sky_polygons_indeces,
		level_data.curved_surfaces , level_data.curved_surfaces_vertices );

	plbTransformCoordinatesFromQuakeSystem(
		level_data.polygons, level_data.vertices,
		level_data.polygons_indeces, level_data.sky_polygons_indeces );

	TransformCurves( level_data.curved_surfaces , level_data.curved_surfaces_vertices );

	GenCurvesNormalizedLightmapCoords( level_data.curved_surfaces , level_data.curved_surfaces_vertices );

	ParseEntities();
	GetBSPLights( level_data.point_lights, level_data.cone_lights );
}
