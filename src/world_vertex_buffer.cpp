#include <iostream>

#include "curves.hpp"

#include "world_vertex_buffer.hpp"

struct LightTexelVertex
{
	float pos[3];
	float lightmap_pos[2];

	char normal[3];
	unsigned char reserved;

	unsigned char tex_maps[4];
};

static void GenPolygonsVerticesNormals(
	const plb_Polygons& in_polygons,
	const plb_Vertices& in_vertices,
	plb_Normals& out_normals )
{
	out_normals.resize( out_normals.size() + in_vertices.size() );

	plb_Normal* n_p= out_normals.data() + out_normals.size() - in_vertices.size();

	for( const plb_Polygon& poly : in_polygons )
	{
		plb_Normal normal;
		for( unsigned int c= 0; c< 3; c++ )
			normal.xyz[c]= (char)(poly.normal[c] * 127.0f);
		for( unsigned int v= poly.first_vertex_number; v< poly.first_vertex_number + poly.vertex_count; v++ )
			n_p[v]= normal;
	}
}

void plb_WorldVertexBuffer::SetupLevelVertexAttributes( r_GLSLProgram& shader )
{
	shader.SetAttribLocation( "pos", Attrib::Pos );
	shader.SetAttribLocation( "tex_coord", Attrib::TexCoord );
	shader.SetAttribLocation( "lightmap_coord", Attrib::LightmapCoord );
	shader.SetAttribLocation( "normal", Attrib::Normal );
	shader.SetAttribLocation( "tex_maps", Attrib::TexMaps );
}

plb_WorldVertexBuffer::plb_WorldVertexBuffer(
	const plb_LevelData& level_data,
	const unsigned int* lightmap_atlas_size,
	const SampleCorrectionFunc& sample_correction_finc )
{
	plb_Vertices combined_vertices;
	plb_Normals normals;
	std::vector<unsigned int> index_buffer;

	PrepareWorldCommonPolygons( level_data, combined_vertices, normals, index_buffer );
	PrepareAlphaShadowPolygons( level_data, combined_vertices, normals, index_buffer );
	PrepareSkyPolygons( level_data, combined_vertices, normals, index_buffer );
	PrepareLuminousPolygons( level_data, combined_vertices, normals, index_buffer );

	// Load to GPU
	polygon_buffer_.VertexData(
		combined_vertices.data(),
		combined_vertices.size() * sizeof(plb_Vertex), sizeof(plb_Vertex) );

	polygon_buffer_.IndexData( index_buffer.data(), index_buffer.size() * sizeof(unsigned int), GL_UNSIGNED_INT, GL_TRIANGLES );

	plb_Vertex v; unsigned int offset;
	offset= ((char*)v.pos) - ((char*)&v);
	polygon_buffer_.VertexAttribPointer( Attrib::Pos, 3, GL_FLOAT, false, offset );
	offset= ((char*)v.tex_coord) - ((char*)&v);
	polygon_buffer_.VertexAttribPointer( Attrib::TexCoord, 2, GL_FLOAT, false, offset );
	offset= ((char*)v.lightmap_coord) - ((char*)&v);
	polygon_buffer_.VertexAttribPointer( Attrib::LightmapCoord, 2, GL_FLOAT, false, offset );
	offset= ((char*)&v.tex_maps[0]) - ((char*)&v);
	polygon_buffer_.VertexAttribPointerInt( Attrib::TexMaps, 3, GL_UNSIGNED_BYTE, offset );

	glGenBuffers( 1, &normals_buffer_id_ );
	glBindBuffer( GL_ARRAY_BUFFER, normals_buffer_id_ );
	glBufferData( GL_ARRAY_BUFFER, normals.size() * sizeof(plb_Normal), normals.data(), GL_STATIC_DRAW );

	glEnableVertexAttribArray( Attrib::Normal );
	glVertexAttribPointer( Attrib::Normal, 3, GL_BYTE, true, sizeof(plb_Normal), NULL );

	PrepareLightTexelsPoints( level_data, lightmap_atlas_size, sample_correction_finc );
}

plb_WorldVertexBuffer::~plb_WorldVertexBuffer()
{
	glDeleteBuffers( 1, &normals_buffer_id_ );
}

void plb_WorldVertexBuffer::DrawLightmapTexels() const
{
	light_texels_points_.Bind();
	light_texels_points_.Draw();
}

void plb_WorldVertexBuffer::Draw( PolygonType type ) const
{
	Draw( 1u << static_cast<unsigned int>(type) );
}

void plb_WorldVertexBuffer::Draw( const std::initializer_list<PolygonType>& types ) const
{
	unsigned int mask= 0;
	for( PolygonType type : types )
		mask|= 1u << static_cast<unsigned int>( type );

	Draw( mask );
}

void plb_WorldVertexBuffer::Draw( const unsigned int polygon_types_flags ) const
{
	polygon_buffer_.Bind();

	for( unsigned int i= 0; i < static_cast<unsigned int>(PolygonType::NumTypes); i++ )
	{
		if( polygon_types_flags & ( 1 << i ) )
		{
			glDrawElements(
				GL_TRIANGLES,
				polygon_groups_[i].size,
				GL_UNSIGNED_INT,
				reinterpret_cast<GLvoid*>( polygon_groups_[i].offset * sizeof(unsigned int) ) );
		}
	}
}

void plb_WorldVertexBuffer::PrepareLightTexelsPoints(
	const plb_LevelData& level_data,
	const unsigned int* lightmap_atlas_size,
	const SampleCorrectionFunc& sample_correction_finc )
{
	std::vector<LightTexelVertex> vertices;

	for( const plb_Polygon& poly : level_data.polygons )
	{
		unsigned int first_vertex= vertices.size();
		vertices.resize( vertices.size() + poly.lightmap_data.size[0] * poly.lightmap_data.size[1] );

		for( unsigned int y= 0; y < poly.lightmap_data.size[1]; y++ )
		for( unsigned int x= 0; x < poly.lightmap_data.size[0]; x++ )
		{
			const m_Vec3 pos=
				m_Vec3( poly.lightmap_pos ) +
				( float(x) + 0.5f ) * m_Vec3( poly.lightmap_basis[0] ) +
				( float(y) + 0.5f ) * m_Vec3( poly.lightmap_basis[1] );

			const m_Vec3 pos_corrected= sample_correction_finc( pos, poly );

			LightTexelVertex& v= vertices[ first_vertex + x + y * poly.lightmap_data.size[0] ];

			for( unsigned int i= 0; i < 3; i++ )
			{
				v.pos[i]= pos_corrected.ToArr()[i];
				v.normal[i]= static_cast<char>( 127.0f * poly.normal[i] );
			}

			v.lightmap_pos[0]= float( poly.lightmap_data.coord[0] + x ) + 0.5f;
			v.lightmap_pos[1]= float( poly.lightmap_data.coord[1] + y ) + 0.5f;
			for( unsigned int i= 0; i < 2; i++ )
				v.lightmap_pos[i]/= float(lightmap_atlas_size[i]);

			v.tex_maps[2]= poly.lightmap_data.atlas_id;
		}
	} // for polygons

	std::vector<PositionAndNormal> curve_coords;
	for( const plb_CurvedSurface& curve : level_data.curved_surfaces )
	{
		curve_coords.resize( curve.lightmap_data.size[0] * curve.lightmap_data.size[1] );

		const m_Vec2 lightmap_coord_scaler{
			float(lightmap_atlas_size[0]),
			float(lightmap_atlas_size[1]) };

		const m_Vec2 lightmap_coord_shift{
			-float(curve.lightmap_data.coord[0]) ,
			-float(curve.lightmap_data.coord[1]) };

		const unsigned int curve_lightmap_size[2]=
			{ curve.lightmap_data.size[0], curve.lightmap_data.size[1] };

		CalculateCurveCoordinatesForLightTexels(
			curve,
			lightmap_coord_scaler, lightmap_coord_shift,
			curve_lightmap_size,
			level_data.curved_surfaces_vertices,
			curve_coords.data() );

		for( unsigned int y= 0; y < curve.lightmap_data.size[1]; y++ )
		for( unsigned int x= 0; x < curve.lightmap_data.size[0]; x++ )
		{
			const PositionAndNormal& position_and_normal=
				curve_coords[ x + y * curve.lightmap_data.size[0] ];

			if( position_and_normal.normal.Length() < 0.01f )
				continue; // Bad texel

			vertices.emplace_back();
			LightTexelVertex& v= vertices.back();

			for( unsigned int i= 0; i < 3; i++ )
			{
				v.pos[i]= position_and_normal.pos.ToArr()[i];
				v.normal[i]= static_cast<char>( 127.0f * position_and_normal.normal.ToArr()[i] );
			}

			v.lightmap_pos[0]= float( curve.lightmap_data.coord[0] + x ) + 0.5f;
			v.lightmap_pos[1]= float( curve.lightmap_data.coord[1] + y ) + 0.5f;
			for( unsigned int i= 0; i < 2; i++ )
				v.lightmap_pos[i]/= float(lightmap_atlas_size[i]);

			v.tex_maps[2]= curve.lightmap_data.atlas_id;

		} // for xy
	} // for curves

	std::cout << "Primary lightmap texels: " << vertices.size() << std::endl;

	light_texels_points_.VertexData(
		vertices.data(),
		vertices.size() * sizeof(LightTexelVertex),
		sizeof(LightTexelVertex) );

	LightTexelVertex v;

	light_texels_points_.VertexAttribPointer(
		Attrib::Pos,
		3, GL_FLOAT, false,
		((char*)v.pos) - ((char*)&v) );

	// May pos be tex_coord too
	light_texels_points_.VertexAttribPointer(
		Attrib::TexCoord,
		3, GL_FLOAT, false,
		((char*)v.pos) - ((char*)&v) );

	light_texels_points_.VertexAttribPointer(
		Attrib::LightmapCoord,
		2, GL_FLOAT, false,
		((char*)v.lightmap_pos) - ((char*)&v) );

	light_texels_points_.VertexAttribPointer(
		Attrib::Normal,
		3, GL_BYTE, false,
		((char*)v.normal) - ((char*)&v) );

	light_texels_points_.VertexAttribPointerInt(
		Attrib::TexMaps,
		4, GL_UNSIGNED_BYTE,
		((char*)v.tex_maps) - ((char*)&v) );

	light_texels_points_.SetPrimitiveType( GL_POINTS );
}

void plb_WorldVertexBuffer::PrepareWorldCommonPolygons(
	const plb_LevelData& level_data,
	plb_Vertices& vertices,
	plb_Normals& normals,
	std::vector<unsigned int>& indeces )
{
	const unsigned int index_cout_before= indeces.size();
	polygon_groups_[ int(PolygonType::WorldCommon) ].offset= indeces.size();

	vertices.insert( vertices.end(), level_data.vertices.begin(), level_data.vertices.end() );

	GenPolygonsVerticesNormals( level_data.polygons, level_data.vertices, normals );

	for( const plb_Polygon& poly : level_data.polygons )
	{
		const plb_Material& material= level_data.materials[ poly.material_id ];
		if( material.cast_alpha_shadow )
			continue;

		indeces.insert(
			indeces.end(),
			level_data.polygons_indeces.begin() + poly.first_index,
			level_data.polygons_indeces.begin() + poly.first_index + poly.index_count );
	} // for polygons

	for( const plb_CurvedSurface& curve : level_data.curved_surfaces )
	{
		const plb_Material& material= level_data.materials[ curve.material_id ];
		if( material.cast_alpha_shadow )
			continue;

		GenCurveMesh( curve, level_data.curved_surfaces_vertices, vertices, indeces, normals );
	} // for curves

	polygon_groups_[ int(PolygonType::WorldCommon) ].size= indeces.size() - index_cout_before;
}

void plb_WorldVertexBuffer::PrepareAlphaShadowPolygons(
	const plb_LevelData& level_data,
	plb_Vertices& vertices,
	plb_Normals& normals,
	std::vector<unsigned int>& indeces )
{
	const unsigned int index_cout_before= indeces.size();
	polygon_groups_[ int(PolygonType::AlphaShadow) ].offset= indeces.size();

	for( const plb_Polygon& poly : level_data.polygons )
	{
		const plb_Material& material= level_data.materials[ poly.material_id ];
		if( !material.cast_alpha_shadow )
			continue;

		indeces.insert(
			indeces.end(),
			level_data.polygons_indeces.begin() + poly.first_index,
			level_data.polygons_indeces.begin() + poly.first_index + poly.index_count );
	} // for polygons

	for( const plb_CurvedSurface& curve : level_data.curved_surfaces )
	{
		const plb_Material& material= level_data.materials[ curve.material_id ];
		if( !material.cast_alpha_shadow )
			continue;

		GenCurveMesh( curve, level_data.curved_surfaces_vertices, vertices, indeces, normals );
	} // for curves

	polygon_groups_[ int(PolygonType::AlphaShadow) ].size= indeces.size() - index_cout_before;
}

void plb_WorldVertexBuffer::PrepareSkyPolygons(
	const plb_LevelData& level_data,
	plb_Vertices& vertices,
	plb_Normals& normals,
	std::vector<unsigned int>& indeces )
{
	(void) vertices;
	(void) normals;

	polygon_groups_[ int(PolygonType::Sky) ].offset= indeces.size();
	polygon_groups_[ int(PolygonType::Sky) ].size= level_data.sky_polygons_indeces.size();

	indeces.insert( indeces.end(), level_data.sky_polygons_indeces.begin(), level_data.sky_polygons_indeces.end() );

	for( const plb_Polygon& poly : level_data.sky_polygons )
	{
		const plb_Material& material= level_data.materials[ poly.material_id ];
		const plb_ImageInfo& texture=
			level_data.textures[ material.light_texture_number ];

		for( unsigned int v= 0; v < poly.vertex_count; v++ )
		{
			plb_Vertex& vertex= vertices[ poly.first_vertex_number + v ];

			vertex.lightmap_coord[0]= material.luminosity;

			vertex.tex_maps[0]= texture.texture_array_id;
			vertex.tex_maps[1]= texture.texture_layer_id;
			vertex.tex_maps[2]= 255; // We do not need lightmap layer for polygons of this type
		} // for polygon vertices
	} // forsky  polygons
}

void plb_WorldVertexBuffer::PrepareLuminousPolygons(
	const plb_LevelData& level_data,
	plb_Vertices& vertices,
	plb_Normals& normals,
	std::vector<unsigned int>& indeces )
{
	const unsigned int index_cout_before= indeces.size();
	polygon_groups_[ int(PolygonType::Luminous) ].offset= indeces.size();

	for( const plb_Polygon& poly : level_data.polygons )
	{
		const plb_Material& material= level_data.materials[ poly.material_id ];
		if( !( material.luminosity > 0.0f && !material.split_to_point_lights ) )
			continue;

		const plb_ImageInfo& texture=
			level_data.textures[ level_data.materials[ poly.material_id ].light_texture_number ];

		const unsigned int first_vertex= vertices.size();
		vertices.resize( vertices.size() + poly.vertex_count );
		normals.resize( vertices.size() + poly.vertex_count );

		plb_Vertex* const v= vertices.data() + vertices.size() - poly.vertex_count;
		plb_Normal* const n= normals.data() + normals.size() - poly.vertex_count;

		// Copy vertices. Write luminosity to lightmap_coord[0], setup tex_maps.
		for( unsigned int i= 0; i < poly.vertex_count; i++ )
		{
			v[i]= level_data.vertices[ poly.first_vertex_number + i ];

			v[i].lightmap_coord[0]= material.luminosity;

			v[i].tex_maps[0]= texture.texture_array_id;
			v[i].tex_maps[1]= texture.texture_layer_id;
			v[i].tex_maps[2]= 255; // We do not need lightmap layer for polygons of this type

			// We do not need normal
			n[i].xyz[0]= n[i].xyz[1]= n[i].xyz[2]= 0;
		}

		// Make new indeces for new vertices
		indeces.resize( indeces.size() + poly.index_count );
		unsigned int* const index= indeces.data() + indeces.size() - poly.index_count;
		for( unsigned int i= 0; i < poly.index_count; i++ )
		{
			index[i]=
				( level_data.polygons_indeces[ poly.first_index + i ] - poly.first_vertex_number ) +
				first_vertex;
		}
	} // for polygons

	for( const plb_CurvedSurface& curve : level_data.curved_surfaces )
	{
		const plb_Material& material= level_data.materials[ curve.material_id ];
		if( material.luminosity <= 0.0f )
			continue;

		const plb_ImageInfo& texture= level_data.textures[ material.light_texture_number ];

		const unsigned int vertices_before= vertices.size();
		GenCurveMesh( curve, level_data.curved_surfaces_vertices, vertices, indeces, normals );

		for( unsigned int v= vertices_before; v < vertices.size(); v++ )
		{
			plb_Vertex& vertex= vertices[v];

			vertex.lightmap_coord[0]= material.luminosity;

			vertex.tex_maps[0]= texture.texture_array_id;
			vertex.tex_maps[1]= texture.texture_layer_id;
			vertex.tex_maps[2]= 255; // We do not need lightmap layer for polygons of this type
		}
	} // for curves

	polygon_groups_[ int(PolygonType::Luminous) ].size= indeces.size() - index_cout_before;
}
