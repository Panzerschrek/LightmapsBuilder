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
	plb_Vertices combined_vertices( level_data.vertices );
	plb_Normals normals;
	std::vector<unsigned int> index_buffer( level_data.polygons_indeces );

	GenPolygonsVerticesNormals( level_data.polygons, level_data.vertices, normals );

	if( level_data.curved_surfaces.size() > 0 )
		GenCurvesMeshes( level_data.curved_surfaces, level_data.curved_surfaces_vertices,
			combined_vertices, index_buffer, normals );

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
}

plb_WorldVertexBuffer::~plb_WorldVertexBuffer()
{
	glDeleteBuffers( 1, &normals_buffer_id_ );
}

void plb_WorldVertexBuffer::Draw() const
{
	polygon_buffer_.Bind();
	polygon_buffer_.Draw();
}
