#include <cmath>
#include <cstring>
#include <iostream>

#include "formats.hpp"

#include "q3_bsp_loader.hpp"

extern "C"
{
typedef unsigned char byte;
typedef int qboolean;
#include <common/mathlib.h>
#include <common/bspfile.h>

}

#include <vec.hpp>

#define Q_UNITS_IN_METER 64.0f
#define INV_Q_UNITS_IN_METER 0.015625f
#define Q3_LIGHTMAP_SIZE 128

static void GetTextures( std::vector<plb_ImageInfo>& out_textures )
{
	out_textures.reserve( numShaders );
	for( const dshader_t* shader= dshaders; shader < dshaders + numShaders; shader++ )
	{
		plb_ImageInfo img;
		img.file_name= std::string( shader->shader );
		out_textures.push_back(img);
	}
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
	std::vector<plb_Polygon>& out_polygons, std::vector<unsigned int>& out_indeces,
	std::vector<plb_CurvedSurface>& out_curves, std::vector<plb_Vertex>& out_curves_vertices )
{
	out_polygons.reserve( numDrawSurfaces );

	for( const dsurface_t* p= drawSurfaces; p < drawSurfaces + numDrawSurfaces; p++ )
	{
		if( p->surfaceType == 1 // polygon, no model or patch
			&& (dshaders[p->shaderNum].surfaceFlags & (SURF_SKY|SURF_NOLIGHTMAP|SURF_NODRAW) ) == 0
			&& (dshaders[p->shaderNum].contentFlags & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_LAVA|CONTENTS_FOG) ) == 0
			)
		{
			const int index_offset= out_indeces.size();
			out_indeces.resize( index_offset + p->numIndexes );
			for( int i= 0; i < p->numIndexes; i++ )
				out_indeces[i + index_offset]= drawIndexes[i + p->firstIndex] + p->firstVert;

			plb_Polygon polygon;

			polygon.first_vertex_number= p->firstVert;
			polygon.vertex_count= p->numVerts;

			polygon.first_index= p->firstIndex;
			polygon.index_count= p->numIndexes;

			polygon.texture_id= p->shaderNum;

			polygon.normal[0]= p->lightmapVecs[2][0];
			polygon.normal[1]= p->lightmapVecs[2][1];
			polygon.normal[2]= p->lightmapVecs[2][2];

			polygon.lightmap_basis[0][0]= p->lightmapVecs[0][0];
			polygon.lightmap_basis[0][1]= p->lightmapVecs[0][1];
			polygon.lightmap_basis[0][2]= p->lightmapVecs[0][2];
			polygon.lightmap_basis[1][0]= p->lightmapVecs[1][0];
			polygon.lightmap_basis[1][1]= p->lightmapVecs[1][1];
			polygon.lightmap_basis[1][2]= p->lightmapVecs[1][2];
			polygon.lightmap_pos[0]= p->lightmapOrigin[0];
			polygon.lightmap_pos[1]= p->lightmapOrigin[1];
			polygon.lightmap_pos[2]= p->lightmapOrigin[2];

			out_polygons.push_back(polygon);
		}// if normal polygon
		else if( p->surfaceType == 2 )// curve 
		{
			plb_CurvedSurface surf;
			surf.grid_size[0]= p->patchWidth;
			surf.grid_size[1]= p->patchHeight;
			surf.first_vertex_number= out_curves_vertices.size();

			surf.texture_id= p->shaderNum;

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

static void TransformPolygons(
	plb_Polygons& polygons,
	plb_Vertices& vertices )
{
	// transform vertices
	for( plb_Vertex& v : vertices )
	{
		std::swap(v.pos[1], v.pos[2]);
		v.pos[0]*= INV_Q_UNITS_IN_METER;
		v.pos[1]*= INV_Q_UNITS_IN_METER;
		v.pos[2]*= INV_Q_UNITS_IN_METER;
	}

	plb_Vertex* v_p= vertices.data();

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

		for( unsigned int v= p.first_vertex_number, v2= p.first_vertex_number + p.vertex_count - 1;
			 v< p.first_vertex_number + p.vertex_count/2;
			v++, v2-- )
			std::swap( v_p[v], v_p[v2] );
	}
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
	for( unsigned int i= 0; i< 3; i++ )
	{
		while(IsCharLexemSeparator(*str)) str++;
		int c;
		sscanf( str, "%d", &c );
		if( c > 255 ) c= 255; if( c < 0 ) c= 0;
		out_color[i]= c;
		while(!IsCharLexemSeparator(*str)) str++;
	}
}

static void ParseColorF( const char* str, unsigned char* out_color )
{
	for( unsigned int i= 0; i< 3; i++ )
	{
		while(IsCharLexemSeparator(*str)) str++;
		float c;
		sscanf( str, "%f", &c );
		c*= 255.0f;
		if( c > 255.0f ) c= 255.0f; if( c < 0.0f ) c= 0.0f;
		out_color[i]= (unsigned char)(c);
		while(!IsCharLexemSeparator(*str)) str++;
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

void LoadQ3Bsp( const char* file_name, plb_LevelData* level_data )
{
	LoadBSPFile( file_name );

	// std::cout << (file_data + header->lumps[ LUMP_ENTITIES ].fileofs );

	LoadVertices( level_data->vertices );

	GetTextures( level_data->textures );
	BuildPolygons(
		level_data->polygons, level_data->polygons_indeces,
		level_data->curved_surfaces , level_data->curved_surfaces_vertices );
	TransformPolygons(level_data->polygons, level_data->vertices );
	TransformCurves( level_data->curved_surfaces , level_data->curved_surfaces_vertices );

	GenCurvesNormalizedLightmapCoords( level_data->curved_surfaces , level_data->curved_surfaces_vertices );

	ParseEntities();
	GetBSPLights( level_data->point_lights, level_data->cone_lights );
}
