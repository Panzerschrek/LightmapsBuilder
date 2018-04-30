#pragma once
#include <functional>

#include <glsl_program.hpp>
#include <polygon_buffer.hpp>
#include "tracer.hpp"

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
		// Geometry with lightmap, and casted solid shadow.
		WorldCommon= 0,
		// Geometry with per-vertex light and casted solid shadow.
		VertexLighted,
		// Geometry with per-vertex light and casted alpha-shadow.
		VertexLightedAlphaShadow,
		// Geometry without shadows and luminosity.
		NoShadow,
		// Geometry with or without lightmap, casted alpha-shadow.
		AlphaShadow,
		// Sky geometry. Directional lights goes from it. Also, sky geometry is lumonous and not cast shadow.
		Sky,
		// Geometry of luminous surfaces, duplicates some geometry from WorldCommon or AlphaShadow.
		Luminous,
		// Luminous geometry without shadows.
		NoShadowLuminous,

		NumTypes
	};

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

	typedef std::function<bool( const plb_LevelModel& model )> ModelAcceptFunction;

private:

	void PrepareWorldCommonPolygons(
		const plb_LevelData& level_data,
		plb_Vertices& vertices,
		plb_Normals& normals,
		std::vector<unsigned int>& indeces );

	void PrepareVertexLightedPolygons(
		const plb_LevelData& level_data,
		plb_Vertices& vertices,
		plb_Normals& normals,
		std::vector<unsigned int>& indeces );

	void PrepareVertexLightedAlphaShadowPolygons(
		const plb_LevelData& level_data,
		plb_Vertices& vertices,
		plb_Normals& normals,
		std::vector<unsigned int>& indeces );

	void PrepareNoShadowPolygons(
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

	void PrepareNoShadowLuminousPolygons(
		const plb_LevelData& level_data,
		plb_Vertices& vertices,
		plb_Normals& normals,
		std::vector<unsigned int>& indeces );

	void PrepareModelsPolygons(
		const plb_LevelData& level_data,
		plb_Vertices& vertices,
		plb_Normals& normals,
		std::vector<unsigned int>& indeces,
		const ModelAcceptFunction& model_accept_function );

private:
	r_PolygonBuffer polygon_buffer_;
	GLuint normals_buffer_id_;

	PolygonGroup polygon_groups_[ static_cast<size_t>(PolygonType::NumTypes) ];
};
