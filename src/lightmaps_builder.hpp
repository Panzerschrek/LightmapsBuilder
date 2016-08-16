#pragma once

#include <glsl_program.hpp>
#include <matrix.hpp>
#include <panzer_ogl_lib.hpp>
#include <polygon_buffer.hpp>
#include <texture.hpp>
#include <vec.hpp>

#include "textures_manager.hpp"

#include "formats.hpp"

#define PLB_MAX_LIGHT_PASSES 8

class plb_LightmapsBuilder
{
public:
	plb_LightmapsBuilder( const char* file_name, const plb_Config* config );
	~plb_LightmapsBuilder();

	void DrawPreview( const m_Mat4* view_matrix, const m_Vec3& cam_pos= m_Vec3(0.0f,0.0f,0.0f) );

private:
	void LoadLightPassShaders();

	void CreateShadowmapCubemap();
	void GenPointlightShadowmap( const m_Vec3& light_pos );
	void PointLightPass(const m_Vec3& light_pos, const m_Vec3& light_color);

	void GenSecondaryLightPassCubemap();
	void SecondaryLightPass( const m_Vec3& pos, const m_Vec3& normal );

	void CreateDirectionalLightShadowmap();
	void GenDirectionalLightShadowmap( const plb_DirectionalLight& light );
	void DirectionalLightPass( const plb_DirectionalLight& light );

	void BuildLightmapBasises();
	void DevideLongPolygons();

	void TransformTexturesCoordinates();

	void ClalulateLightmapAtlasCoordinates();
	void CreateLightmapBuffers();

	void FillBorderLightmapTexels();

	void CalculateLevelBoundingBox();
private:
	plb_LevelData level_data_;
	struct
	{
		m_Vec3 min;
		m_Vec3 max;
	} level_bounding_box_;

	plb_Config config_;

	r_GLSLProgram polygons_preview_shader_;

	// VBO dlä obycnyh poliginov i poligonov krivyh poverhnostej
	r_PolygonBuffer polygons_vbo_;
	GLuint polygon_vbo_vertex_normals_vbo_;


	r_GLSLProgram normals_shader_;
	r_PolygonBuffer normals_vbo_;

	struct
	{
		unsigned int size[3];
		GLuint colored_test_tex_id;

		// FBO for first pass
		GLuint tex_id;
		GLuint fbo_id;

		GLuint secondary_tex_id[ PLB_MAX_LIGHT_PASSES ];
		GLuint secondary_tex_fbo; // use 1 FBO and switch between them
	} lightmap_atlas_texture_;

	struct
	{
		unsigned int size; // for cubemap texture is square
		GLuint depth_tex_id;
		GLuint fbo_id;
		float max_light_distance;
	} point_light_shadowmap_cubemap_;
	r_GLSLProgram point_light_pass_shader_;
	r_GLSLProgram point_light_shadowmap_shader_;

	struct
	{
		unsigned int size;
		GLuint tex_id;
		GLuint depth_tex_id;
		GLuint fbo_id;
		/*
		+------+---+---+
		|   up |  down |
		+------+---+---+
		|      |le |ri |
		|front |ft |ght|
		+------+---+---+
		*/
		GLuint direction_multipler_tex_id;
		unsigned int direction_multipler_tex_scaler;
	} secondary_light_pass_cubemap_;
	r_GLSLProgram secondary_light_pass_shader_;


	struct
	{
		unsigned int size[2];
		GLuint depth_tex_id;
		GLuint fbo_id;
		m_Mat4 view_matrix;
		m_Vec3 light_direction;
		float z_min, z_max;
	} directional_light_shadowmap_;

	r_GLSLProgram shadowmap_shader_;
	r_GLSLProgram directional_light_pass_shader_;

	r_GLSLProgram texture_show_shader_;
	r_PolygonBuffer cubemap_show_buffer_;

	unsigned int viewport_size_[2];

	plb_TexturesManager* textures_manager_;
};
