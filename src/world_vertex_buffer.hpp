#pragma once

#include <glsl_program.hpp>
#include <polygon_buffer.hpp>

#include "formats.hpp"

class plb_WorldVertexBuffer final
{
public:
	struct Attrib
	{
		enum
		{
			Pos= 0,
			TexCoord,
			LightmapCoord,
			Normal,
			TexMaps,
		};
	};

	static void SetupLevelVertexAttributes( r_GLSLProgram& shader );

	explicit plb_WorldVertexBuffer( const plb_LevelData& level_data );
	~plb_WorldVertexBuffer();

	void Draw() const;

private:
	r_PolygonBuffer polygon_buffer_;
	GLuint normals_buffer_id_;
};
