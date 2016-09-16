#pragma once
#include <functional>
#include <memory>

#include <glsl_program.hpp>
#include <framebuffer.hpp>
#include <matrix.hpp>
#include <panzer_ogl_lib.hpp>
#include <polygon_buffer.hpp>
#include <texture.hpp>
#include <vec.hpp>

#include "formats.hpp"
#include "lights_visualizer.hpp"
#include "textures_manager.hpp"
#include "tracer.hpp"
#include "world_vertex_buffer.hpp"

#define PLB_MAX_LIGHT_PASSES 8

class plb_LightmapsBuilder final
{
public:
	plb_LightmapsBuilder( const char* file_name, const plb_Config& config );
	~plb_LightmapsBuilder();

	void MakeBrightLuminousSurfacesLight( const std::function<void()>& wake_up_callback );

	void MakeSecondaryLight( const std::function<void()>& wake_up_callback );

	void DrawPreview(
		const m_Mat4& view_matrix, const m_Vec3& cam_pos,
		const m_Vec3& cam_dir,
		float brightness,
		bool show_primary_lightmap, bool show_secondary_lightmap, bool show_textures );

private:
	void LoadLightPassShaders();

	void CreateShadowmapCubemap();
	void GenPointlightShadowmap( const m_Vec3& light_pos );
	void PointLightPass( const m_Vec3& light_pos, const m_Vec3& light_color );
	void SurfaceSampleLightPass( const m_Vec3& light_pos, const m_Vec3& light_normal, const m_Vec3& light_color );

	void GenSecondaryLightPassCubemap();
	void GenSecondaryLightPassUnwrapBuffer();
	void SecondaryLightPass( const m_Vec3& pos, const m_Vec3& normal );

	void GenDirectionalLightShadowmap( const m_Mat4& shadow_mat );
	void DirectionalLightPass( const plb_DirectionalLight& light, const m_Mat4& shadow_mat );

	void GenConeLightShadowmap( const m_Mat4& shadow_mat );
	void ConeLightPass( const plb_ConeLight& light, const m_Mat4& shadow_mat );

	void MarkLuminousMaterials();

	void BuildLuminousSurfacesLights();

	// Builds lightmap basises from texture basises.
	// Needs only for input data without lightmap basises.
	void BuildLightmapBasises();

	void DevideLongPolygons();

	void TransformTexturesCoordinates();

	void ClalulateLightmapAtlasCoordinates();
	void CreateLightmapBuffers();

	void PrepareLightTexelsPoints();

	void FillBorderLightmapTexels();

	void CalculateLevelBoundingBox();

	m_Vec3 CorrectSecondaryLightSample(
		const m_Vec3& pos,
		const plb_Polygon& poly,
		const plb_Tracer::LineSegments& neighbors_segments );

	void GetPolygonNeighborsSegments(
		const plb_Polygon& polygon,
		plb_Tracer::SurfacesList& tmp_surfaces_container,
		plb_Tracer::LineSegments& out_segments );

private:
	plb_LevelData level_data_;
	struct
	{
		m_Vec3 min;
		m_Vec3 max;
	} level_bounding_box_;

	plb_SurfaceSampleLights bright_luminous_surfaces_lights_;

	const plb_Config config_;

	r_GLSLProgram polygons_preview_shader_;
	r_GLSLProgram polygons_preview_alphatested_shader_;

	struct
	{
		unsigned int size[3];
		GLuint colored_test_tex_id;

		// FBO for first pass
		GLuint tex_id;
		GLuint fbo_id;

		unsigned int secondary_lightmap_size[2];
		GLuint secondary_tex_id[ PLB_MAX_LIGHT_PASSES ];
		GLuint secondary_tex_fbo; // use 1 FBO and switch between them
	} lightmap_atlas_texture_;

	r_PolygonBuffer light_texels_points_;

	struct
	{
		unsigned int size; // for cubemap texture is square
		GLuint depth_tex_id;
		GLuint fbo_id;
		float max_light_distance;
	} point_light_shadowmap_cubemap_;
	r_GLSLProgram point_light_pass_shader_;
	r_GLSLProgram surface_sample_light_pass_shader_;
	r_GLSLProgram point_light_shadowmap_shader_;
	r_GLSLProgram point_light_shadowmap_alphatested_shader_;

	struct
	{
		unsigned int size;
		GLuint tex_id;
		GLuint depth_tex_id;
		GLuint fbo_id;

		GLuint direction_multipler_tex_id;
		unsigned int direction_multipler_tex_scaler;
		float direction_multiplier_normalizer;

		r_GLSLProgram unwrap_shader;
		r_PolygonBuffer unwrap_geometry;
		r_Framebuffer unwrap_framebuffer;

		r_GLSLProgram write_shader;

	} secondary_light_pass_cubemap_;
	r_GLSLProgram secondary_light_pass_shader_;
	r_GLSLProgram secondary_light_pass_shader_luminocity_shader_;
	r_GLSLProgram secondary_light_pass_shader_alphatested_shader_;

	r_Framebuffer directional_light_shadowmap_;

	r_GLSLProgram shadowmap_shader_; // common with cone light
	r_GLSLProgram shadowmap_alphatested_shader_; // common with cone light
	r_GLSLProgram directional_light_sky_mark_shader_;
	r_GLSLProgram directional_light_pass_shader_;

	r_Framebuffer cone_light_shadowmap_;

	r_GLSLProgram cone_light_pass_shader_;

	r_GLSLProgram texture_show_shader_;
	r_PolygonBuffer cubemap_show_buffer_;

	std::unique_ptr<plb_TexturesManager> textures_manager_;
	std::unique_ptr<plb_WorldVertexBuffer> world_vertex_buffer_;
	std::unique_ptr<plb_Tracer> tracer_;
	std::unique_ptr<plb_LightsVisualizer> lights_visualizer_;
};
