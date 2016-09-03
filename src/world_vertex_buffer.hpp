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

	enum class PolygonType : unsigned int
	{
		WorldCommon= 0,
		AlphaShadow,
		Sky,
		Luminous,
		NumTypes
	};
/*
private:
	// Transform enum values to bitmask
	template<class Arg0, class... Args>
	static unsigned int GetFlags( PolygonType arg0, Args... args )
	{
		return ( 1 << static_cast<unsigned int>(arg0) ) | GetFlags( args... );
	}
*/
public:
	static void SetupLevelVertexAttributes( r_GLSLProgram& shader );

	explicit plb_WorldVertexBuffer( const plb_LevelData& level_data );
	~plb_WorldVertexBuffer();

	void Draw( PolygonType type ) const;
	void Draw( const std::initializer_list<PolygonType>& types ) const;
	void Draw( unsigned int polygon_types_flags ) const;

private:
	struct PolygonGroup
	{
		unsigned int offset;
		unsigned int size;
	};

private:
	void PrepareWorldCommonPolygons(
		const plb_LevelData& level_data,
		plb_Vertices& vertices,
		plb_Normals& normals,
		std::vector<unsigned int>& indeces );

	void PrepareAlphaShadowPolygons(
		const plb_LevelData& level_data,
		plb_Vertices& vertices,
		plb_Normals& normals,
		std::vector<unsigned int>& indeces );

	void PrepareSkyPolygons(
		 const plb_LevelData& level_data,
		plb_Vertices& vertices,
		plb_Normals& normals,
		std::vector<unsigned int>& indeces );

	void PrepareLuminousPolygons(
		 const plb_LevelData& level_data,
		plb_Vertices& vertices,
		plb_Normals& normals,
		std::vector<unsigned int>& indeces );

private:
	r_PolygonBuffer polygon_buffer_;
	GLuint normals_buffer_id_;

	PolygonGroup polygon_groups_[ static_cast<size_t>(PolygonType::NumTypes) ];

};
