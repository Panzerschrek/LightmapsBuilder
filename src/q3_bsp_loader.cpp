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

static void GetTextures( const std::vector<dshader_t>* in_textues, std::vector<plb_ImageInfo>* out_textures )
{
	out_textures->reserve( in_textues->size() );
	for( std::vector<dshader_t>::const_iterator t= in_textues->begin(); t< in_textues->end(); t++ )
	{
		plb_ImageInfo img;
		img.file_name= std::string( t->shader );
		out_textures->push_back(img);
	}
}

static void BuildPolygons(
	const std::vector<dsurface_t>* in_surfaces, const std::vector<drawVert_t>* in_vertices, const std::vector<dshader_t>* in_textures,
	std::vector<plb_Polygon>* out_polygons, std::vector<plb_Vertex>* out_vertices,
	std::vector<plb_CurvedSurface>* out_curves, std::vector<plb_Vertex>* out_curves_vertices )
{
	const dshader_t* shader_p= &*in_textures->begin();

	out_vertices->reserve( in_vertices->size() * 3 / 2 );
	out_polygons->reserve( in_surfaces->size() );

	const drawVert_t* v_p= &*in_vertices->begin();

	for( std::vector<dsurface_t>::const_iterator p= in_surfaces->begin(); p< in_surfaces->end(); p++ )
	{
		if( p->surfaceType == 1 // polygon, no model or patch
			&& (shader_p[p->shaderNum].surfaceFlags & (SURF_SKY|SURF_NOLIGHTMAP|SURF_NODRAW) ) == 0
			&& (shader_p[p->shaderNum].contentFlags & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_LAVA|CONTENTS_FOG) ) == 0
			)
		{
			plb_Polygon polygon;

			unsigned int vert_delta= (p->numIndexes)/3 + 2 - p->numVerts;
			// make individual vertices for polygon. in some cases, in vertex list for polyon is center point for polygon
			for( unsigned int v= p->firstVert; v< ((unsigned int)p->firstVert + p->numVerts - vert_delta); v++ )
			{
				plb_Vertex vertex;
				vertex.pos[0]= v_p[v].xyz[0];
				vertex.pos[1]= v_p[v].xyz[1];
				vertex.pos[2]= v_p[v].xyz[2];
				vertex.tex_coord[0]= v_p[v].st[0];
				vertex.tex_coord[1]= v_p[v].st[1];

				vertex.lightmap_coord[0]= v_p[v].lightmap[0];
				vertex.lightmap_coord[1]= v_p[v].lightmap[1];

				out_vertices->push_back(vertex);
			}// make vertices

			polygon.first_vertex_number= out_vertices->size() - (p->numVerts - vert_delta);
			polygon.vertex_count= p->numVerts - vert_delta;

			polygon.texture_id= p->shaderNum;

			polygon.normal[0]= p->lightmapVecs[2][0];
			polygon.normal[1]= p->lightmapVecs[2][1];
			polygon.normal[2]= p->lightmapVecs[2][2];

			polygon.lightmap_basis[0][0]= p->lightmapVecs[0][0];
			polygon.lightmap_basis[0][1]= p->lightmapVecs[0][1];
			polygon.lightmap_basis[0][2]= p->lightmapVecs[0][2];
			polygon.lightmap_basis[0][3]= -m_Vec3(p->lightmapOrigin) * m_Vec3( p->lightmapVecs[0] );
			polygon.lightmap_basis[1][0]= p->lightmapVecs[1][0];
			polygon.lightmap_basis[1][1]= p->lightmapVecs[1][1];
			polygon.lightmap_basis[1][2]= p->lightmapVecs[1][2];
			polygon.lightmap_basis[1][3]= -m_Vec3(p->lightmapOrigin) * m_Vec3( p->lightmapVecs[1] );

			out_polygons->push_back(polygon);
		}// if normal polygon
		else if( p->surfaceType == 2 )// curve 
		{
			plb_CurvedSurface surf;
			surf.grid_size[0]= p->patchWidth;
			surf.grid_size[1]= p->patchHeight;
			surf.first_vertex_number= out_curves_vertices->size();

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

				vert.pos[0]= v_p[v_ind].xyz[0];
				vert.pos[1]= v_p[v_ind].xyz[1];
				vert.pos[2]= v_p[v_ind].xyz[2];
				vert.tex_coord[0]= v_p[v_ind].st[0];
				vert.tex_coord[1]= v_p[v_ind].st[1];
				vert.lightmap_coord[0]= v_p[v_ind].lightmap[0];
				vert.lightmap_coord[1]= v_p[v_ind].lightmap[1];

				out_curves_vertices->push_back(vert);
			}// for patch control vertices

			out_curves->push_back(surf);
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

		float k= 1.0f / 1.0f;
		p.lightmap_basis[0][0]*= k;
		p.lightmap_basis[0][1]*= k;
		p.lightmap_basis[0][2]*= k;
		p.lightmap_basis[0][3]*= INV_Q_UNITS_IN_METER * k;
		p.lightmap_basis[1][0]*= k;
		p.lightmap_basis[1][1]*= k;
		p.lightmap_basis[1][2]*= k;
		p.lightmap_basis[1][3]*= INV_Q_UNITS_IN_METER * k;

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
	std::vector<plb_CurvedSurface>* curves,
	std::vector<plb_Vertex>* curves_vertices )
{
	const float infinity= 1e16f;
	const float eps= 1e-3f;

	plb_Vertex* v_p= &*curves_vertices->begin();

	for( std::vector<plb_CurvedSurface>::iterator curve= curves->begin(); curve< curves->end(); curve++ )
	{
		float uv_min[]= {  infinity,  infinity };
		float uv_max[]= { -infinity, -infinity };
		for( unsigned int v= 0; v< curve->grid_size[0] * curve->grid_size[1]; v++ )
		{
			plb_Vertex* vert= v_p + curve->first_vertex_number + v;
			if( vert->lightmap_coord[0] > uv_max[0] ) uv_max[0]= vert->lightmap_coord[0];
			else if( vert->lightmap_coord[0] < uv_min[0] ) uv_min[0]= vert->lightmap_coord[0];
			if( vert->lightmap_coord[1] > uv_max[1] ) uv_max[1]= vert->lightmap_coord[1];
			else if( vert->lightmap_coord[1] < uv_min[1] ) uv_min[1]= vert->lightmap_coord[1];
		}
		float inv_size[]= { uv_max[0] - uv_min[0], uv_max[1] - uv_min[1] };

		if( inv_size[0] < eps || inv_size[1] < eps )
		{
			for( unsigned int v= 0; v< curve->grid_size[0] * curve->grid_size[1]; v++ )
			{
				plb_Vertex* vert= v_p + curve->first_vertex_number + v;
				vert->lightmap_coord[0]= 0.0f;
				vert->lightmap_coord[1]= 0.0f;
			}
		}
		else
		{
			inv_size[0]= 1.0f / inv_size[0];
			inv_size[1]= 1.0f / inv_size[1];

			for( unsigned int v= 0; v< curve->grid_size[0] * curve->grid_size[1]; v++ )
			{
				plb_Vertex* vert= v_p + curve->first_vertex_number + v;
				vert->lightmap_coord[0]= (vert->lightmap_coord[0]- uv_min[0]) * inv_size[0];
				vert->lightmap_coord[1]= (vert->lightmap_coord[1]- uv_min[1]) * inv_size[1];
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
	FILE* f= fopen( file_name, "rb" );
	if( f == NULL )
	{
	}

	fseek(f, 0, SEEK_END);
	int file_size= ftell(f);
	fseek(f, 0, SEEK_SET);

	char* file_data= new char[ file_size ];
	fread( file_data, file_size, 1, f );
	fclose(f);

	dheader_t* header= (dheader_t*)file_data;

	std::vector<dshader_t> textures;
	std::vector<dsurface_t> surfaces;
	std::vector<drawVert_t> vertices;

	textures.resize( header->lumps[ LUMP_SHADERS ].filelen / sizeof(dshader_t) );
	memcpy( &*textures.begin(), file_data + header->lumps[ LUMP_SHADERS ].fileofs, header->lumps[ LUMP_SHADERS ].filelen );

	surfaces.resize( header->lumps[ LUMP_SURFACES ].filelen / sizeof(dsurface_t) );
	memcpy( &*surfaces.begin(), file_data + header->lumps[ LUMP_SURFACES ].fileofs, header->lumps[ LUMP_SURFACES ].filelen );

	vertices.resize( header->lumps[ LUMP_DRAWVERTS ].filelen / sizeof(drawVert_t) );
	memcpy( &*vertices.begin(), file_data + header->lumps[ LUMP_DRAWVERTS ].fileofs, header->lumps[ LUMP_DRAWVERTS ].filelen );

	// std::cout << (file_data + header->lumps[ LUMP_ENTITIES ].fileofs );

	GetTextures( &textures, &level_data->textures );
	BuildPolygons( &surfaces, &vertices, &textures,
		&level_data->polygons, &level_data->vertices,
		&level_data->curved_surfaces , &level_data->curved_surfaces_vertices );
	TransformPolygons(level_data->polygons, level_data->vertices );
	TransformCurves( level_data->curved_surfaces , level_data->curved_surfaces_vertices );

	if( level_data->curved_surfaces .size() > 0 )
		GenCurvesNormalizedLightmapCoords( &level_data->curved_surfaces , &level_data->curved_surfaces_vertices );

	LoadBSPFile( file_name );
	ParseEntities();
	GetBSPLights( level_data->point_lights, level_data->cone_lights );

	delete[] file_data;
}
