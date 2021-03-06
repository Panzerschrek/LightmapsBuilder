#include <cstring>
#include <iostream>

#include "curves.hpp"

#include "world_vertex_buffer.hpp"

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

plb_WorldVertexBuffer::plb_WorldVertexBuffer( const plb_LevelData& level_data )
{
	plb_Vertices combined_vertices;
	plb_Normals normals;
	std::vector<unsigned int> index_buffer;

	PrepareWorldCommonPolygons( level_data, combined_vertices, normals, index_buffer );
	PrepareVertexLightedPolygons( level_data, combined_vertices, normals, index_buffer );
	PrepareVertexLightedAlphaShadowPolygons( level_data, combined_vertices, normals, index_buffer );
	PrepareNoShadowPolygons( level_data, combined_vertices, normals, index_buffer );
	PrepareAlphaShadowPolygons( level_data, combined_vertices, normals, index_buffer );
	PrepareSkyPolygons( level_data, combined_vertices, normals, index_buffer );
	PrepareLuminousPolygons( level_data, combined_vertices, normals, index_buffer );
	PrepareNoShadowLuminousPolygons( level_data, combined_vertices, normals, index_buffer );

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

	std::cout << "Total triangles in world: " << index_buffer.size() / 3u << std::endl;
}

plb_WorldVertexBuffer::~plb_WorldVertexBuffer()
{
	glDeleteBuffers( 1, &normals_buffer_id_ );
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
		if( ( poly.flags & plb_SurfaceFlags::NoShadow ) != 0 )
			continue;

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
		if( ( curve.flags & plb_SurfaceFlags::NoShadow ) != 0 )
			continue;

		const plb_Material& material= level_data.materials[ curve.material_id ];
		if( material.cast_alpha_shadow )
			continue;

		GenCurveMesh( curve, level_data.curved_surfaces_vertices, vertices, indeces, normals );
	} // for curves

	polygon_groups_[ int(PolygonType::WorldCommon) ].size= indeces.size() - index_cout_before;
}

void plb_WorldVertexBuffer::PrepareVertexLightedPolygons(
	const plb_LevelData& level_data,
	plb_Vertices& vertices,
	plb_Normals& normals,
	std::vector<unsigned int>& indeces )
{
	const unsigned int index_cout_before= indeces.size();
	polygon_groups_[ int(PolygonType::VertexLighted) ].offset= indeces.size();

	PrepareModelsPolygons(
		level_data,
		vertices,
		normals,
		indeces,
		[this, &level_data]( const plb_LevelModel& model ) -> bool
		{
			const plb_Material& material= level_data.materials[ model.material_id ];

			return
				!material.cast_alpha_shadow &&
				( model.flags & plb_SurfaceFlags::NoShadow ) == 0 &&
				( model.flags & plb_SurfaceFlags::NoLightmap ) == 0;
		} );

	polygon_groups_[ int(PolygonType::VertexLighted) ].size= indeces.size() - index_cout_before;
}

void plb_WorldVertexBuffer::PrepareVertexLightedAlphaShadowPolygons(
	const plb_LevelData& level_data,
	plb_Vertices& vertices,
	plb_Normals& normals,
	std::vector<unsigned int>& indeces )
{
	const unsigned int index_cout_before= indeces.size();
	polygon_groups_[ int(PolygonType::VertexLightedAlphaShadow) ].offset= indeces.size();

	PrepareModelsPolygons(
		level_data,
		vertices,
		normals,
		indeces,
		[this, &level_data]( const plb_LevelModel& model ) -> bool
		{
			const plb_Material& material= level_data.materials[ model.material_id ];

			return
				material.cast_alpha_shadow &&
				( model.flags & plb_SurfaceFlags::NoShadow ) == 0 &&
				( model.flags & plb_SurfaceFlags::NoLightmap ) == 0;
		} );

	polygon_groups_[ int(PolygonType::VertexLightedAlphaShadow) ].size= indeces.size() - index_cout_before;
}

void plb_WorldVertexBuffer::PrepareNoShadowPolygons(
	const plb_LevelData& level_data,
	plb_Vertices& vertices,
	plb_Normals& normals,
	std::vector<unsigned int>& indeces )
{
	const unsigned int index_cout_before= indeces.size();
	polygon_groups_[ int(PolygonType::NoShadow) ].offset= indeces.size();

	for( const plb_Polygon& poly : level_data.polygons )
	{
		if( ( poly.flags & plb_SurfaceFlags::NoShadow ) == 0 )
			continue;

		const plb_Material& material= level_data.materials[ poly.material_id ];
		if( material.cast_alpha_shadow ||
			( material.luminosity > 0.0f && !material.split_to_point_lights ) )
			continue;

		indeces.insert(
			indeces.end(),
			level_data.polygons_indeces.begin() + poly.first_index,
			level_data.polygons_indeces.begin() + poly.first_index + poly.index_count );
	} // for polygons

	for( const plb_CurvedSurface& curve : level_data.curved_surfaces )
	{
		if( ( curve.flags & plb_SurfaceFlags::NoShadow ) == 0 )
			continue;

		const plb_Material& material= level_data.materials[ curve.material_id ];
		if( material.cast_alpha_shadow || material.luminosity > 0.0f )
			continue;

		GenCurveMesh( curve, level_data.curved_surfaces_vertices, vertices, indeces, normals );
	} // for curves

	PrepareModelsPolygons(
		level_data,
		vertices,
		normals,
		indeces,
		[this, &level_data]( const plb_LevelModel& model ) -> bool
		{
			if( ( model.flags & plb_SurfaceFlags::NoShadow ) == 0 )
				return false;

			const plb_Material& material= level_data.materials[ model.material_id ];
			return material.luminosity <= 0.0f;
		} );

	polygon_groups_[ int(PolygonType::NoShadow) ].size= indeces.size() - index_cout_before;
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

	PrepareModelsPolygons(
		level_data,
		vertices,
		normals,
		indeces,
		[this, &level_data]( const plb_LevelModel& model ) -> bool
		{
			const plb_Material& material= level_data.materials[ model.material_id ];
			return material.cast_alpha_shadow;
		} );

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
		if( ( poly.flags & plb_SurfaceFlags::NoShadow ) != 0 )
			continue;

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
		if( ( curve.flags & plb_SurfaceFlags::NoShadow ) != 0 )
			continue;

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

	PrepareModelsPolygons(
		level_data,
		vertices,
		normals,
		indeces,
		[this, &level_data]( const plb_LevelModel& model ) -> bool
		{
			if( ( model.flags & plb_SurfaceFlags::NoShadow ) != 0 )
				return false;

			const plb_Material& material= level_data.materials[ model.material_id ];
			return material.luminosity > 0.0f;
		} );

	polygon_groups_[ int(PolygonType::Luminous) ].size= indeces.size() - index_cout_before;
}

void plb_WorldVertexBuffer::PrepareNoShadowLuminousPolygons(
	const plb_LevelData& level_data,
	plb_Vertices& vertices,
	plb_Normals& normals,
	std::vector<unsigned int>& indeces )
{
	const unsigned int index_cout_before= indeces.size();
	polygon_groups_[ int(PolygonType::NoShadowLuminous) ].offset= indeces.size();

	for( const plb_Polygon& poly : level_data.polygons )
	{
		if( ( poly.flags & plb_SurfaceFlags::NoShadow ) == 0 )
			continue;

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
		if( ( curve.flags & plb_SurfaceFlags::NoShadow ) == 0 )
			continue;

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

	PrepareModelsPolygons(
		level_data,
		vertices,
		normals,
		indeces,
		[this, &level_data]( const plb_LevelModel& model ) -> bool
		{
			if( ( model.flags & plb_SurfaceFlags::NoShadow ) == 0 )
				return false;

			const plb_Material& material= level_data.materials[ model.material_id ];
			return material.luminosity > 0.0f;
		} );

	polygon_groups_[ int(PolygonType::NoShadowLuminous) ].size= indeces.size() - index_cout_before;
}

void plb_WorldVertexBuffer::PrepareModelsPolygons(
	const plb_LevelData& level_data,
	plb_Vertices& vertices,
	plb_Normals& normals,
	std::vector<unsigned int>& indeces,
	const ModelAcceptFunction& model_accept_function )
{
	for( const plb_LevelModel& model : level_data.models )
	{
		if( !model_accept_function( model ) )
			continue;

		const unsigned int first_out_vertex= vertices.size();

		for( unsigned int i= 0; i < model.index_count; i++ )
		{
			indeces.push_back(
				level_data.models_indeces[ model.first_index + i ] +
				first_out_vertex - model.first_vertex_number );
		}

		const plb_Material& material= level_data.materials[ model.material_id ];
		const plb_ImageInfo& texture= level_data.textures[ material.albedo_texture_number ];

		for( unsigned int v= 0; v < model.vertex_count; v++ )
		{
			vertices.emplace_back();
			normals.emplace_back();

			const plb_Vertex& in_vertex= level_data.models_vertices[ model.first_vertex_number + v ];
			const plb_Normal& in_normal= level_data.models_normals[ model.first_vertex_number + v ];

			plb_Vertex& out_vertex= vertices.back();
			plb_Normal& out_normal= normals.back();

			out_vertex= in_vertex;
			out_normal= in_normal;

			unsigned int material_id;
			std::memcpy( &material_id, in_vertex.tex_maps, sizeof(unsigned int) );

			out_vertex.tex_maps[0]= texture.texture_array_id;
			out_vertex.tex_maps[1]= texture.texture_layer_id;
		}
	} // for models
}
