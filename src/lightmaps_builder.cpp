#include <algorithm>
#include <iostream>
#include <cstring>

#include <shaders_loading.hpp>

#include "lightmaps_builder.hpp"

#include "curves.hpp"
#include "loaders_common.hpp"
#include "math_utils.hpp"
#include "rasterizer.hpp"

#define VEC3_CPY(dst,src) (dst)[0]= (src)[0]; (dst)[1]= (src)[1]; (dst)[2]= (src)[2];
#define ARR_VEC3_CPY(dst,vec) dst[0]= vec.x; dst[1]= vec.y; dst[2]= vec.z;
#define REALLY_MAX_FLOAT 1e24f

struct CubemapGeometryVertex
{
	float vec[3];
	float tex_coord[2];
};

static const r_GLSLVersion g_glsl_version( r_GLSLVersion::KnowmNumbers::v430 );

static const float g_cubemaps_znear= 1.0f / 32.0f;
static const float g_cubemaps_min_clip_distance= g_cubemaps_znear * std::sqrt(3.0f);

static void GenCubemapSideDirectionMultipler( unsigned int size, unsigned char* out_data, unsigned int side_num )
{
	/*
	23
	01
	*/
	static const float side_vectors[]=
	{
		 1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,   1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f, // X+
		-1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f, // X-
		-1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f, // Y+
		 1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,  -1.0f, -1.0f, -1.0f, // Y-
		-1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f, // Z+
		 1.0f, -1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f, // Z-
	};
	const float* const control_vectors[4]=
	{
		side_vectors + side_num * 12,
		side_vectors + side_num * 12 + 3,
		side_vectors + side_num * 12 + 6,
		side_vectors + side_num * 12 + 9
	};

	const float dk= 1.0f / float(size);
	for( unsigned int y= 0; y< size; y++ )
	{
		const float yk= (float(y) + 0.5f) * dk;
		const float yk1= 1.0f - yk;
		m_Vec3 vy[2];
		vy[0]= m_Vec3( control_vectors[0] ) * yk1 + m_Vec3( control_vectors[2] ) * yk;
		vy[1]= m_Vec3( control_vectors[1] ) * yk1 + m_Vec3( control_vectors[3] ) * yk;

		float xk= dk * 0.5f;
		float xk1= 1.0f - xk;
		for( unsigned int x= 0; x< size; x++, xk+= dk, xk1-= dk )
		{
			const m_Vec3 vec= vy[0] * xk1 + vy[1] * xk;
			const m_Vec3 side_vec( xk * 2.0f - 1.0f, yk * 2.0f - 1.0f, 1.0f );
			const float f= 255.0f * vec.z / ( vec.Length() * side_vec.Length() );
			
			int fi= int( std::round(f) );
			if( fi > 255 ) fi= 255;
			else if( fi < 0 ) fi= 0;
			out_data[ x + y * size ]= fi;
		}// for x
	}// for y
}

static void Setup2dShadowmap( r_Framebuffer& shadowmap_fbo, unsigned int size )
{
	shadowmap_fbo=
		r_Framebuffer(
			std::vector<r_Texture::PixelFormat>(), // No color textures
			r_Texture::PixelFormat::Depth16,
			size,
			size );

	r_Texture& depth_texture= shadowmap_fbo.GetDepthTexture();

	depth_texture.SetCompareMode( r_Texture::CompareMode::Less );
	depth_texture.SetWrapMode( r_Texture::WrapMode::Clamp );
	depth_texture.SetFiltration( r_Texture::Filtration::Linear, r_Texture::Filtration::Linear );
}

static void CreateRotationMatrixForDirection(
	const m_Vec3& dir,
	m_Mat4& out_mat )
{
	const float c_sin_eps= 0.995f;

		 if( dir.y >=  c_sin_eps )
		out_mat.RotateX(  plb_Constants::half_pi );
	else if( dir.y <= -c_sin_eps )
		out_mat.RotateX( -plb_Constants::half_pi );
	else
	{
		m_Mat4 rotate_x, rotate_y;
		const float angle_x= std::asin( dir.y );
		const float angle_y= std::atan2( dir.x, dir.z );

		rotate_x.RotateX( angle_x);
		rotate_y.RotateY(-angle_y);

		out_mat= rotate_y * rotate_x;
	}
}

static void CreateDirectionalLightMatrix(
	const plb_DirectionalLight& light,
	const m_Vec3& bb_min,
	const m_Vec3& bb_max,
	m_Mat4& out_mat )
{
	m_Mat4 projection, rotation, shift;

	m_Vec3 dir( light.direction );
	dir.Normalize();

	rotation.RotateX( -plb_Constants::half_pi );

	shift.Identity();
	shift[4]= -dir.x / dir.y;
	shift[6]= -dir.z / dir.y;

	rotation= shift * rotation;

	const float inf= 1e24f;
	m_Vec3 proj_min( inf, inf, inf ), proj_max( -inf, -inf, -inf );
	for( unsigned int i= 0; i< 8; i++ )
	{
		m_Vec3 v;
		v.x= (i&1) ? bb_max.x : bb_min.x;
		v.y= ((i>>1)&1) ? bb_max.y : bb_min.y;
		v.z= ((i>>2)&1) ? bb_max.z : bb_min.z;
		v= v * rotation;
		for( unsigned int j= 0; j< 3; j++ )
		{
			if( v.ToArr()[j] > proj_max.ToArr()[j] ) proj_max.ToArr()[j]= v.ToArr()[j];
			else if( v.ToArr()[j] < proj_min.ToArr()[j] ) proj_min.ToArr()[j]= v.ToArr()[j];
		}
	}

	projection.Identity();
	projection[ 0]= 2.0f / ( proj_max.x - proj_min.x );
	projection[12]= 1.0f - proj_max.x * projection[ 0];
	projection[ 5]= 2.0f / ( proj_max.y - proj_min.y );
	projection[13]= 1.0f - proj_max.y * projection[ 5];
	projection[10]= 2.0f / ( proj_max.z - proj_min.z );
	projection[14]= 1.0f - proj_max.z * projection[10];

	out_mat= rotation * projection;
}

static void CreateConeLightMatrix(
	const plb_ConeLight& light,
	m_Mat4& out_mat )
{
	m_Mat4 translate, rotate, perspective;

	translate.Translate( -m_Vec3(light.pos) );

	CreateRotationMatrixForDirection( m_Vec3(light.direction), rotate );

	perspective.PerspectiveProjection( 1.0f, 2.0f * light.angle, 0.1f, 256.0f );

	out_mat= translate * rotate * perspective;
}

// Axis-aligned cubemap
static void GenCubemapMatrices( const m_Vec3& pos, m_Mat4* out_matrices )
{
	m_Mat4 perspective, shift;
	perspective.PerspectiveProjection( 1.0f, plb_Constants::half_pi, g_cubemaps_znear, 256.0f );
	shift.Translate( -pos );

	m_Mat4 tmp; tmp.RotateZ( plb_Constants::pi );
	out_matrices[0].RotateY( plb_Constants::half_pi );
	out_matrices[1].RotateY( -plb_Constants::half_pi );
	out_matrices[2].RotateX( -plb_Constants::half_pi );
	out_matrices[2]*= tmp;
	out_matrices[3].RotateX( plb_Constants::half_pi );
	out_matrices[3]*= tmp;
	out_matrices[4].RotateY( -plb_Constants::pi );
	out_matrices[5].Identity();

	for( unsigned int i= 0; i< 6; i++ )
		out_matrices[i]= shift * out_matrices[i] * perspective;
}

// Align cubemap, using direction
static void GenCubemapMatrices( const m_Vec3& pos, const m_Vec3& dir, m_Mat4* out_matrices )
{
	m_Mat4 perspective, shift, rotate, rotate_and_shift;

	perspective.PerspectiveProjection( 1.0f, plb_Constants::half_pi, g_cubemaps_znear, 256.0f );
	shift.Translate( -pos );
	CreateRotationMatrixForDirection( dir, rotate );

	m_Mat4 tmp; tmp.RotateZ( plb_Constants::pi );
	out_matrices[0].RotateY( plb_Constants::half_pi );
	out_matrices[1].RotateY( -plb_Constants::half_pi );
	out_matrices[2].RotateX( -plb_Constants::half_pi );
	out_matrices[2]*= tmp;
	out_matrices[3].RotateX( plb_Constants::half_pi );
	out_matrices[3]*= tmp;
	out_matrices[4].RotateY( -plb_Constants::pi );
	out_matrices[5].Identity();

	rotate_and_shift= shift * rotate;

	for( unsigned int i= 0; i< 6; i++ )
		out_matrices[i]= rotate_and_shift * out_matrices[i] * perspective;
}

plb_LightmapsBuilder::plb_LightmapsBuilder( const char* file_name, const plb_Config& config )
	: config_( config )
{
	LoadBsp( file_name , config_, level_data_ );

	textures_manager_.reset(
		new plb_TexturesManager(
			config_,
			level_data_.textures, level_data_.build_in_images ) );

	MarkLuminousMaterials();

	ClalulateLightmapAtlasCoordinates();
	CreateLightmapBuffers();
	TransformTexturesCoordinates();
	CalculateLevelBoundingBox();

	BuildLuminousSurfacesLights();

	lights_visualizer_.reset(
		new plb_LightsVisualizer(
			level_data_.point_lights,
			level_data_.cone_lights,
			bright_luminous_surfaces_lights_ ) );

	tracer_.reset( new plb_Tracer( level_data_ ) );

	world_vertex_buffer_.reset(
		new plb_WorldVertexBuffer(
			level_data_,
			lightmap_atlas_texture_.size,
			[this]( const m_Vec3& pos, const plb_Polygon& poly )
			{
				return CorrectSecondaryLightSample( pos, poly );
			}) );

	polygons_preview_shader_.ShaderSource(
		rLoadShader( "preview_f.glsl", g_glsl_version),
		rLoadShader( "preview_v.glsl", g_glsl_version) );
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(polygons_preview_shader_);
	polygons_preview_shader_.Create();

	polygons_preview_alphatested_shader_.ShaderSource(
		rLoadShader( "preview_f.glsl", g_glsl_version, { "ALPHA_TEST" } ),
		rLoadShader( "preview_v.glsl", g_glsl_version) );
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(polygons_preview_alphatested_shader_);
	polygons_preview_alphatested_shader_.Create();

	LoadLightPassShaders();
	CreateShadowmapCubemap();
	Setup2dShadowmap( directional_light_shadowmap_, 1 << config_.directional_light_shadowmap_size_log2 );
	Setup2dShadowmap( cone_light_shadowmap_, 1 << config_.cone_light_shadowmap_size_log2 );

	for( unsigned int i= 0; i< level_data_.point_lights.size(); i++ )
	{
		m_Vec3 light_pos;
		m_Vec3 light_color;
		unsigned char max_color_component= 1;
		for( int j= 0; j< 3; j++ )
		{
			unsigned char c= level_data_.point_lights[i].color[j];
			light_color.ToArr()[j]= level_data_.point_lights[i].intensity * float(c) / 255.0f;
			if( c > max_color_component ) max_color_component= c;
			light_pos.ToArr()[j]= level_data_.point_lights[i].pos[j];
		}
		light_color/= float(max_color_component) / 255.0f;
		
		GenPointlightShadowmap( light_pos );
		PointLightPass( light_pos, light_color );
	}

	for( const plb_DirectionalLight& light : level_data_.directional_lights )
	{
		m_Mat4 mat;
		CreateDirectionalLightMatrix(
			light,
			level_bounding_box_.min,
			level_bounding_box_.max,
			mat );

		GenDirectionalLightShadowmap( mat );
		DirectionalLightPass( light, mat );
	}

	for( const plb_ConeLight& cone_light : level_data_.cone_lights )
	{
		m_Mat4 mat;
		CreateConeLightMatrix( cone_light, mat );

		GenConeLightShadowmap( mat );
		ConeLightPass( cone_light, mat );
	}

	//FillBorderLightmapTexels();

	GenSecondaryLightPassCubemap();
	GenSecondaryLightPassUnwrapBuffer();
}

plb_LightmapsBuilder::~plb_LightmapsBuilder()
{
}

void plb_LightmapsBuilder::MakeBrightLuminousSurfacesLight(
	const std::function<void()>& wake_up_callback )
{
	unsigned int count= 0;

	for( const plb_SurfaceSampleLight& light : bright_luminous_surfaces_lights_ )
	{
		m_Vec3 light_color;

		for( int j= 0; j< 3; j++ )
			light_color.ToArr()[j]= light.intensity * float(light.color[j]) / 255.0f;

		GenPointlightShadowmap( m_Vec3( light.pos ) );
		SurfaceSampleLightPass(
			m_Vec3( light.pos ),
			m_Vec3( light.normal ),
			light_color );

		count++;
		if( count == 30 )
		{
			wake_up_callback();
			count= 0;
		}
	}
}

void plb_LightmapsBuilder::MakeSecondaryLight( const std::function<void()>& wake_up_callback )
{
	unsigned int counter= 0;

	glGenFramebuffers( 1, &lightmap_atlas_texture_.secondary_tex_fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, lightmap_atlas_texture_.secondary_tex_fbo );
	glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , lightmap_atlas_texture_.secondary_tex_id[0], 0 );
	const GLuint color_attachment= GL_COLOR_ATTACHMENT0;
	glDrawBuffers( 1, &color_attachment );

	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );

	secondary_light_pass_cubemap_.write_shader.Bind();
	secondary_light_pass_cubemap_.write_shader.Uniform( "tex", int(0) );
	secondary_light_pass_cubemap_.write_shader.Uniform( "mip", int(config_.secondary_light_pass_cubemap_size_log2) );
	secondary_light_pass_cubemap_.write_shader.Uniform(
		"normalizer", secondary_light_pass_cubemap_.direction_multiplier_normalizer );

	for( const plb_Polygon& poly : level_data_.polygons )
	{
		const m_Vec3 normal(poly.normal);

		const unsigned int sx=
			( poly.lightmap_data.size[0] + config_.secondary_lightmap_scaler - 1 ) /
			config_.secondary_lightmap_scaler;
		const unsigned int sy=
			( poly.lightmap_data.size[1] + config_.secondary_lightmap_scaler - 1 ) /
			config_.secondary_lightmap_scaler;

		const float basis_scale= float(config_.secondary_lightmap_scaler);

		for( unsigned int y= 0; y < sy; y++ )
		for( unsigned int x= 0; x < sx; x++ )
		{
			const m_Vec3 pos=
				( float(x) + 0.5f ) * basis_scale * m_Vec3(poly.lightmap_basis[0]) +
				( float(y) + 0.5f ) * basis_scale * m_Vec3(poly.lightmap_basis[1]) +
				m_Vec3( poly.lightmap_pos );

			const m_Vec3 pos_corrected= CorrectSecondaryLightSample( pos, poly );
			SecondaryLightPass( pos_corrected, normal );

			glBindFramebuffer( GL_FRAMEBUFFER, lightmap_atlas_texture_.secondary_tex_fbo );
			glViewport(
				0, 0,
				lightmap_atlas_texture_.secondary_lightmap_size[0],
				lightmap_atlas_texture_.secondary_lightmap_size[1] );

			const float tc_x=
				( float( x + poly.lightmap_data.coord[0] / config_.secondary_lightmap_scaler ) + 0.5f ) /
				float(lightmap_atlas_texture_.secondary_lightmap_size[0]);
			const float tc_y=
				( float( y + poly.lightmap_data.coord[1] / config_.secondary_lightmap_scaler ) + 0.5f ) /
				float(lightmap_atlas_texture_.secondary_lightmap_size[1]);

			secondary_light_pass_cubemap_.unwrap_framebuffer.GetTextures().front().Bind(0);

			secondary_light_pass_cubemap_.write_shader.Bind();
			secondary_light_pass_cubemap_.write_shader.Uniform(
				"tex_coord",
				m_Vec3( tc_x, tc_y, float(poly.lightmap_data.atlas_id) + 0.01f ) );

			glDrawArrays( GL_POINTS, 0, 1 );

			counter++;
			if( counter >= 100 )
			{
				counter= 0;
				r_Framebuffer::BindScreenFramebuffer();
				wake_up_callback();
				printf( "Polygon : %d/%d\n", &poly - level_data_.polygons.data(), level_data_.polygons.size() );
			}
		}
	}

	std::vector<PositionAndNormal> curve_coords;
	for( const plb_CurvedSurface& curve : level_data_.curved_surfaces )
	{
		const unsigned int lightmap_size[2]=
		{
			( curve.lightmap_data.size[0] + config_.secondary_lightmap_scaler - 1 ) /
				config_.secondary_lightmap_scaler,
			( curve.lightmap_data.size[1] + config_.secondary_lightmap_scaler - 1 ) /
				config_.secondary_lightmap_scaler,
		};

		curve_coords.resize( lightmap_size[0] * lightmap_size[1] );
		std::memset( curve_coords.data(), 0, curve_coords.size() * sizeof(PositionAndNormal) );

		const m_Vec2 lightmap_coord_scaler(
			float(lightmap_atlas_texture_.size[0]) / float(config_.secondary_lightmap_scaler),
			float(lightmap_atlas_texture_.size[1]) / float(config_.secondary_lightmap_scaler) );
		const m_Vec2 lightmap_coord_shift(
				-float(curve.lightmap_data.coord[0] / config_.secondary_lightmap_scaler),
				-float(curve.lightmap_data.coord[1] / config_.secondary_lightmap_scaler));

		CalculateCurveCoordinatesForLightTexels(
			curve,
			lightmap_coord_scaler, lightmap_coord_shift,
			lightmap_size,
			level_data_.curved_surfaces_vertices,
			curve_coords.data() );

		for( unsigned int y= 0; y < lightmap_size[1]; y++ )
		for( unsigned int x= 0; x < lightmap_size[0]; x++ )
		{
			const PositionAndNormal& texel_pos= curve_coords[ x + y * lightmap_size[0] ];

			// Degenerate texel
			if( texel_pos.normal.SquareLength() <= 0.01f )
				continue;

			SecondaryLightPass( texel_pos.pos, texel_pos.normal );

			glBindFramebuffer( GL_FRAMEBUFFER, lightmap_atlas_texture_.secondary_tex_fbo );
			glViewport(
				0, 0,
				lightmap_atlas_texture_.secondary_lightmap_size[0],
				lightmap_atlas_texture_.secondary_lightmap_size[1] );

			const float tc_x=
				( float( x + curve.lightmap_data.coord[0] / config_.secondary_lightmap_scaler ) + 0.5f ) /
				float(lightmap_atlas_texture_.secondary_lightmap_size[0]);
			const float tc_y=
				( float( y + curve.lightmap_data.coord[1] / config_.secondary_lightmap_scaler ) + 0.5f ) /
				float(lightmap_atlas_texture_.secondary_lightmap_size[1]);

			secondary_light_pass_cubemap_.unwrap_framebuffer.GetTextures().front().Bind(0);

			secondary_light_pass_cubemap_.write_shader.Bind();
			secondary_light_pass_cubemap_.write_shader.Uniform(
				"tex_coord",
				m_Vec3( tc_x, tc_y, float(curve.lightmap_data.atlas_id) + 0.01f ) );

			glDrawArrays( GL_POINTS, 0, 1 );

			counter++;
			if( counter >= 100 )
			{
				counter= 0;
				r_Framebuffer::BindScreenFramebuffer();
				wake_up_callback();
			}
		}
	}

	r_Framebuffer::BindScreenFramebuffer();
}

void plb_LightmapsBuilder::DrawPreview(
	const m_Mat4& view_matrix, const m_Vec3& cam_pos,
	const m_Vec3& cam_dir,
	float brightness,
	bool show_primary_lightmap, bool show_secondary_lightmap, bool show_textures )
{
	r_Framebuffer::BindScreenFramebuffer();

	glClearColor ( 0.1f, 0.05f, 0.1f, 0.0f );
	glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glActiveTexture( GL_TEXTURE0 + 0 );
	glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.tex_id );
	glActiveTexture( GL_TEXTURE0 + 1 );
	glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.secondary_tex_id[0] );
	glActiveTexture( GL_TEXTURE0 + 2 );
	glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.colored_test_tex_id );
	glActiveTexture( GL_TEXTURE0 + 3 );
	glBindTexture( GL_TEXTURE_CUBE_MAP, point_light_shadowmap_cubemap_.depth_tex_id );

	glActiveTexture( GL_TEXTURE0 + 0 );

	const auto setup_shader=
	[&](r_GLSLProgram& shader )
	{
		shader.Bind();
		shader.Uniform( "view_matrix", view_matrix );
		shader.Uniform( "lightmap", int(0) );
		shader.Uniform( "secondary_lightmap", int(1) );
		shader.Uniform( "lightmap_test", int(2) );
		shader.Uniform( "cubemap", int(3) );

		shader.Uniform( "brightness", brightness );
		shader.Uniform( "primary_lightmap_scaler", show_primary_lightmap ? 1.0f : 0.0f );
		shader.Uniform( "secondary_lightmap_scaler", show_secondary_lightmap ? 1.0f : 0.0f );
		shader.Uniform( "gray_factor", show_textures ? 0.0f : 1.0f );

		// Correct lightmap coordinates for secondary lightmaps,
		// because size % scaler != 0, sometimes.
		const float tex_scale_x=
			float(lightmap_atlas_texture_.size[0]) /
			float( lightmap_atlas_texture_.secondary_lightmap_size[0] * config_.secondary_lightmap_scaler );
		const float tex_scale_y=
			float(lightmap_atlas_texture_.size[1]) /
			float( lightmap_atlas_texture_.secondary_lightmap_size[1] * config_.secondary_lightmap_scaler );

		shader.Uniform(
			"secondaty_lightmap_tex_coord_scaler",
			m_Vec2(tex_scale_x, tex_scale_y ) );

		unsigned int arrays_bindings_unit= 3;
		textures_manager_->BindTextureArrays(arrays_bindings_unit);
		int textures_uniform[32];
		for( unsigned int i= 0; i< textures_manager_->ArraysCount(); i++ )
			textures_uniform[i]= arrays_bindings_unit + i;
		shader.Uniform( "textures", textures_uniform, textures_manager_->ArraysCount() );
	};

	setup_shader( polygons_preview_shader_ );
	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::WorldCommon );

	setup_shader( polygons_preview_alphatested_shader_ );
	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::AlphaShadow );

	lights_visualizer_->Draw( view_matrix, cam_pos );
	// Debug secondary light pass
	/*
	SecondaryLightPass( cam_pos, cam_dir );

	secondary_light_pass_cubemap_.unwrap_framebuffer.GetTextures().front().Bind(0);

	texture_show_shader_.Bind();
	texture_show_shader_.Uniform( "tex", int(0) );
	cubemap_show_buffer_.Draw();
	*/
}

void plb_LightmapsBuilder::LoadLightPassShaders()
{
	const std::vector<std::string> alpha_test_defines{ "ALPHA_TEST" };

	point_light_pass_shader_.ShaderSource(
		rLoadShader( "point_light_pass_f.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_v.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_g.glsl", g_glsl_version));
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(point_light_pass_shader_);
	point_light_pass_shader_.Create();


	surface_sample_light_pass_shader_.ShaderSource(
		rLoadShader( "surface_sample_light_pass_f.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_v.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_g.glsl", g_glsl_version));
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(surface_sample_light_pass_shader_);
	surface_sample_light_pass_shader_.Create();

	point_light_shadowmap_shader_.ShaderSource(
		rLoadShader( "point_light_shadowmap_f.glsl", g_glsl_version),
		rLoadShader( "point_light_shadowmap_v.glsl", g_glsl_version),
		rLoadShader( "point_light_shadowmap_g.glsl", g_glsl_version));
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(point_light_shadowmap_shader_);
	point_light_shadowmap_shader_.Create();

	point_light_shadowmap_alphatested_shader_.ShaderSource(
		rLoadShader( "point_light_shadowmap_f.glsl", g_glsl_version, alpha_test_defines ),
		rLoadShader( "point_light_shadowmap_v.glsl", g_glsl_version ),
		rLoadShader( "point_light_shadowmap_g.glsl", g_glsl_version ));
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(point_light_shadowmap_alphatested_shader_);
	point_light_shadowmap_alphatested_shader_.Create();

	secondary_light_pass_shader_.ShaderSource(
		rLoadShader( "secondary_light_pass_f.glsl", g_glsl_version),
		rLoadShader( "secondary_light_pass_v.glsl", g_glsl_version),
		rLoadShader( "secondary_light_pass_g.glsl", g_glsl_version));
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(secondary_light_pass_shader_);
	secondary_light_pass_shader_.Create();

	{
		std::vector<std::string> frag_defines;
		if( config_.use_average_texture_color_for_luminous_surfaces )
			frag_defines.emplace_back( "AVERAGE_LIGHT" );

		secondary_light_pass_shader_luminocity_shader_.ShaderSource(
			rLoadShader( "secondary_light_pass_luminosity_f.glsl", g_glsl_version, frag_defines ),
			rLoadShader( "secondary_light_pass_v.glsl", g_glsl_version ),
			rLoadShader( "secondary_light_pass_g.glsl", g_glsl_version ) );
		plb_WorldVertexBuffer::SetupLevelVertexAttributes(secondary_light_pass_shader_luminocity_shader_);
		secondary_light_pass_shader_luminocity_shader_.Create();
	}

	secondary_light_pass_shader_alphatested_shader_.ShaderSource(
		rLoadShader( "secondary_light_pass_f.glsl", g_glsl_version, alpha_test_defines ),
		rLoadShader( "secondary_light_pass_v.glsl", g_glsl_version ),
		rLoadShader( "secondary_light_pass_g.glsl", g_glsl_version ));
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(secondary_light_pass_shader_alphatested_shader_);
	secondary_light_pass_shader_alphatested_shader_.Create();

	shadowmap_shader_.ShaderSource(
		"", // No fragment shader
		rLoadShader( "shadowmap_v.glsl", g_glsl_version));
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(shadowmap_shader_);
	shadowmap_shader_.Create();

	shadowmap_alphatested_shader_.ShaderSource(
		rLoadShader( "shadowmap_f.glsl", g_glsl_version, alpha_test_defines ),
		rLoadShader( "shadowmap_v.glsl", g_glsl_version));
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(shadowmap_alphatested_shader_);
	shadowmap_alphatested_shader_.Create();

	directional_light_sky_mark_shader_.ShaderSource(
		rLoadShader( "directional_light_sky_mark_f.glsl", g_glsl_version ),
		rLoadShader( "shadowmap_v.glsl", g_glsl_version));
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(directional_light_sky_mark_shader_);
	directional_light_sky_mark_shader_.Create();

	directional_light_pass_shader_.ShaderSource(
		rLoadShader( "sun_light_pass_f.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_v.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_g.glsl", g_glsl_version));
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(directional_light_pass_shader_);
	directional_light_pass_shader_.Create();

	cone_light_pass_shader_.ShaderSource(
		rLoadShader( "cone_light_pass_f.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_v.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_g.glsl", g_glsl_version));
	plb_WorldVertexBuffer::SetupLevelVertexAttributes(cone_light_pass_shader_);
	cone_light_pass_shader_.Create();
}

void plb_LightmapsBuilder::CreateShadowmapCubemap()
{
	point_light_shadowmap_cubemap_.size= 1 << config_.point_light_shadowmap_cubemap_size_log2;
	point_light_shadowmap_cubemap_.max_light_distance= 128.0f;
	const unsigned int texture_size= point_light_shadowmap_cubemap_.size;

	glGenTextures( 1, &point_light_shadowmap_cubemap_.depth_tex_id );
	glBindTexture( GL_TEXTURE_CUBE_MAP, point_light_shadowmap_cubemap_.depth_tex_id );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
	for( unsigned int i= 0; i< 6; i++ )
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT16,
			texture_size, texture_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL );

	glGenFramebuffers( 1, &point_light_shadowmap_cubemap_.fbo_id );
	glBindFramebuffer( GL_FRAMEBUFFER, point_light_shadowmap_cubemap_.fbo_id );
	glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT , point_light_shadowmap_cubemap_.depth_tex_id, 0 );
	GLuint da= GL_NONE;
	glDrawBuffers( 1, &da );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	texture_show_shader_.ShaderSource(
		rLoadShader("texture_show_f.glsl", g_glsl_version),
		rLoadShader("texture_show_v.glsl", g_glsl_version) );
	texture_show_shader_.SetAttribLocation( "pos", 0 );
	texture_show_shader_.SetAttribLocation( "tex_coord", 1 );
	texture_show_shader_.Create();

	// cubemap show buffer
	float cubemap_show_data[ 4 * 6 ];
	const float side_size[]= { 1.5f, 1.0f };
	const float quad_pos[]= { -0.75f, -0.5f };
	// vertex coord
	cubemap_show_data[0 ]= quad_pos[0]        ;//x0
	cubemap_show_data[1 ]= quad_pos[1]        ;//y0
	cubemap_show_data[4 ]= quad_pos[0] + side_size[0];//x1
	cubemap_show_data[5 ]= quad_pos[1]        ;//y1
	cubemap_show_data[8 ]= quad_pos[0] + side_size[0];//x2
	cubemap_show_data[9 ]= quad_pos[1] + side_size[1];//y2
	cubemap_show_data[12]= quad_pos[0]        ;//x3
	cubemap_show_data[13]= quad_pos[1] + side_size[1];//y3
	// texture coord
	cubemap_show_data[2 ]= 0.0f; cubemap_show_data[3 ]= 0.0f; //0
	cubemap_show_data[6 ]= 1.0f; cubemap_show_data[7 ]= 0.0f; //1
	cubemap_show_data[10]= 1.0f; cubemap_show_data[11]= 1.0f; //2
	cubemap_show_data[14]= 0.0f; cubemap_show_data[15]= 1.0f; //3
	// duplicated vertices
	memcpy( cubemap_show_data + 16, cubemap_show_data    , sizeof(float) * 4 ); // v5
	memcpy( cubemap_show_data + 20, cubemap_show_data + 8, sizeof(float) * 4 ); // v6

	cubemap_show_buffer_.VertexData( cubemap_show_data, sizeof(float) * 6 * 4, sizeof(float)*4 );
	cubemap_show_buffer_.VertexAttribPointer( 0, 2, GL_FLOAT, false, 0);
	cubemap_show_buffer_.VertexAttribPointer( 1, 2, GL_FLOAT, false, sizeof(float)*2 );
	cubemap_show_buffer_.SetPrimitiveType(GL_TRIANGLES);
}

void plb_LightmapsBuilder::GenPointlightShadowmap( const m_Vec3& light_pos )
{
	glViewport( 0, 0, point_light_shadowmap_cubemap_.size, point_light_shadowmap_cubemap_.size );

	glBindFramebuffer( GL_FRAMEBUFFER, point_light_shadowmap_cubemap_.fbo_id );
	glClear( GL_DEPTH_BUFFER_BIT );

	m_Mat4 final_matrices[6];
	GenCubemapMatrices( light_pos, final_matrices );
	const float inv_max_light_dst= 1.0f / point_light_shadowmap_cubemap_.max_light_distance;
	
	// Regular geometry
	point_light_shadowmap_shader_.Bind();
	point_light_shadowmap_shader_.Uniform( "view_matrices", final_matrices, 6 );
	point_light_shadowmap_shader_.Uniform( "inv_max_light_dst", inv_max_light_dst );

	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::WorldCommon );

	// Alpha-tested geometry
	glDisable( GL_CULL_FACE );

	point_light_shadowmap_alphatested_shader_.Bind();
	point_light_shadowmap_alphatested_shader_.Uniform( "view_matrices", final_matrices, 6 );
	point_light_shadowmap_alphatested_shader_.Uniform( "inv_max_light_dst", inv_max_light_dst );

	int textures_uniform[32];
	const unsigned int arrays_bindings_unit= 3;
	textures_manager_->BindTextureArrays( arrays_bindings_unit );
	for( unsigned int i= 0; i< textures_manager_->ArraysCount(); i++ )
		textures_uniform[i]= arrays_bindings_unit + i;
	point_light_shadowmap_alphatested_shader_.Uniform( "textures", textures_uniform, textures_manager_->ArraysCount() );

	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::AlphaShadow );

	glEnable( GL_CULL_FACE );

	r_Framebuffer::BindScreenFramebuffer();
}

void plb_LightmapsBuilder::PointLightPass(const m_Vec3& light_pos, const m_Vec3& light_color)
{
	glDisable( GL_CULL_FACE );
	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ONE );

	glViewport( 0, 0, lightmap_atlas_texture_.size[0], lightmap_atlas_texture_.size[1] );
	glBindFramebuffer( GL_FRAMEBUFFER, lightmap_atlas_texture_.fbo_id );

	glActiveTexture( GL_TEXTURE0 + 0 );
	glBindTexture( GL_TEXTURE_CUBE_MAP, point_light_shadowmap_cubemap_.depth_tex_id );

	point_light_pass_shader_.Bind();
	point_light_pass_shader_.Uniform( "light_pos", light_pos );
	point_light_pass_shader_.Uniform( "light_color", light_color );
	point_light_pass_shader_.Uniform( "cubemap", int(0) );
	point_light_pass_shader_.Uniform( "inv_max_light_dst", 1.0f / point_light_shadowmap_cubemap_.max_light_distance );

	world_vertex_buffer_->DrawLightmapTexels();

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	glEnable( GL_CULL_FACE );
	glDisable( GL_BLEND );
}

void plb_LightmapsBuilder::SurfaceSampleLightPass(
	const m_Vec3& light_pos,
	const m_Vec3& light_normal,
	const m_Vec3& light_color )
{
	glDisable( GL_CULL_FACE );
	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ONE );

	glViewport( 0, 0, lightmap_atlas_texture_.size[0], lightmap_atlas_texture_.size[1] );
	glBindFramebuffer( GL_FRAMEBUFFER, lightmap_atlas_texture_.fbo_id );

	glActiveTexture( GL_TEXTURE0 + 0 );
	glBindTexture( GL_TEXTURE_CUBE_MAP, point_light_shadowmap_cubemap_.depth_tex_id );

	surface_sample_light_pass_shader_.Bind();
	surface_sample_light_pass_shader_.Uniform( "light_pos", light_pos );
	surface_sample_light_pass_shader_.Uniform( "light_normal", light_normal );
	surface_sample_light_pass_shader_.Uniform( "light_color", light_color );
	surface_sample_light_pass_shader_.Uniform( "cubemap", int(0) );
	surface_sample_light_pass_shader_.Uniform( "inv_max_light_dst", 1.0f / point_light_shadowmap_cubemap_.max_light_distance );

	world_vertex_buffer_->DrawLightmapTexels();

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	glEnable( GL_CULL_FACE );
	glDisable( GL_BLEND );
}

void plb_LightmapsBuilder::GenSecondaryLightPassCubemap()
{
	secondary_light_pass_cubemap_.size= 1 << config_.secondary_light_pass_cubemap_size_log2;
	secondary_light_pass_cubemap_.direction_multipler_tex_scaler= 2;

	// texture with direction multipler
	glGenTextures( 1, &secondary_light_pass_cubemap_.direction_multipler_tex_id );
	glBindTexture( GL_TEXTURE_CUBE_MAP, secondary_light_pass_cubemap_.direction_multipler_tex_id );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );

	{
		const unsigned int direction_multiplier_cubemap_size=
			secondary_light_pass_cubemap_.size / secondary_light_pass_cubemap_.direction_multipler_tex_scaler;

		std::vector<unsigned char> multipler_data(
			direction_multiplier_cubemap_size * direction_multiplier_cubemap_size );

		unsigned int multiplier_sum= 0;
		for( unsigned int i= 0; i< 6; i++ )
		{
			GenCubemapSideDirectionMultipler(
				direction_multiplier_cubemap_size,
				multipler_data.data(), i );
			multiplier_sum+= std::accumulate( multipler_data.begin(), multipler_data.end(), 0 );

			glTexImage2D(
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_R8,
				direction_multiplier_cubemap_size, direction_multiplier_cubemap_size,
				0, GL_RED, GL_UNSIGNED_BYTE, multipler_data.data() );
		}

		// Devide by hemicube square.
		// 255 - byte color to normalized color.
		secondary_light_pass_cubemap_.direction_multiplier_normalizer=
			float(255 * 3 *direction_multiplier_cubemap_size * direction_multiplier_cubemap_size ) /
			float(multiplier_sum);
	}

	// depth texture
	glGenTextures( 1, &secondary_light_pass_cubemap_.depth_tex_id );
	glBindTexture( GL_TEXTURE_CUBE_MAP, secondary_light_pass_cubemap_.depth_tex_id );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	for( unsigned int i= 0; i< 6; i++ )
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT24,
			secondary_light_pass_cubemap_.size, secondary_light_pass_cubemap_.size,
			0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL );

	// main texture
	glGenTextures( 1, &secondary_light_pass_cubemap_.tex_id );
	glBindTexture( GL_TEXTURE_CUBE_MAP, secondary_light_pass_cubemap_.tex_id );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	for( unsigned int i= 0; i< 6; i++ )
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA32F,
			secondary_light_pass_cubemap_.size, secondary_light_pass_cubemap_.size,
			0, GL_RGBA, GL_FLOAT, NULL );

	glGenFramebuffers( 1, &secondary_light_pass_cubemap_.fbo_id );
	glBindFramebuffer( GL_FRAMEBUFFER, secondary_light_pass_cubemap_.fbo_id );
	glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT , secondary_light_pass_cubemap_.depth_tex_id, 0 );
	glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , secondary_light_pass_cubemap_.tex_id, 0 );
	GLuint color_attachment= GL_COLOR_ATTACHMENT0;
	glDrawBuffers( 1, &color_attachment );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void plb_LightmapsBuilder::GenSecondaryLightPassUnwrapBuffer()
{
	secondary_light_pass_cubemap_.unwrap_shader.ShaderSource(
		rLoadShader( "secondary_light_pass_cubemap_unwrap_f.glsl", g_glsl_version ),
		rLoadShader( "secondary_light_pass_cubemap_unwrap_v.glsl", g_glsl_version ) );
	secondary_light_pass_cubemap_.unwrap_shader.SetAttribLocation( "coord", 0 );
	secondary_light_pass_cubemap_.unwrap_shader.SetAttribLocation( "tex_coord", 1 );
	secondary_light_pass_cubemap_.unwrap_shader.Create();

	/*
	   + -------+
	   |   y+   |
	+--+--------+--+
	|  |        |  |
	|x+|   z+   |x-|
	|  |        |  |
	+--+--------+--+
	   |   y-   |
	   +--------+

	0       1/3  1/2  2/3       1
	+--------+----+----+--------+ 1
	|        |    |    |   y+   |
	|   z+   | x+ | x- |________| 1/2;
	|        |    |    |        |
	|        |    |    |   y-   |
	+--------+----+----+--------+ 0
	 */
	static const float o0= 0.0f;
	static const float o13 = 1.0f / 3.0f;
	static const float o12= 1.0f / 2.0f;
	static const float o23 = 2.0f / 3.0f;
	static const float o1= 1.0f;
	static const CubemapGeometryVertex unwrap_geometry[]=
	{
		// z+ quad
		{ { 1,  1,  1}, {  o0,  o1 } }, // up left
		{ {-1,  1,  1}, { o13,  o1 } }, // up right
		{ {-1, -1,  1}, { o13,  o0 } }, // down right
		{ {-1, -1,  1}, { o13,  o0 } }, // down right
		{ { 1, -1,  1}, {  o0,  o0 } }, // down left
		{ { 1,  1,  1}, {  o0,  o1 } }, // up left
		// x+ quad
		{ { 1,  1,  0}, { o13,  o1 } }, // up left
		{ { 1,  1,  1}, { o12,  o1 } }, // up right
		{ { 1, -1,  1}, { o12,  o0 } }, // down right
		{ { 1, -1,  1}, { o12,  o0 } }, // down right
		{ { 1, -1,  0}, { o13,  o0 } }, // down left
		{ { 1,  1,  0}, { o13,  o1 } }, // up left
		// x- quad
		{ {-1,  1,  1}, { o12,  o1 } },  // up left
		{ {-1,  1,  0}, { o23,  o1 } },  // up right
		{ {-1, -1,  0}, { o23,  o0 } },  // down right
		{ {-1, -1,  0}, { o23,  o0 } },  // down right
		{ {-1, -1,  1}, { o12,  o0 } },  // down left
		{ {-1,  1,  1}, { o12,  o1 } },  // up left
		// y+ quad
		{ { 1,  1,  0}, { o23,  o1 } },  // up left
		{ {-1,  1,  0}, {  o1,  o1 } },  // up right
		{ {-1,  1,  1}, {  o1, o12 } },  // down right
		{ {-1,  1,  1}, {  o1, o12 } },  // down right
		{ { 1,  1,  1}, { o23, o12 } },  // down left
		{ { 1,  1,  0}, { o23,  o1 } },  // up left
		// y- quad
		{ { 1, -1,  1}, { o23, o12 } },  // up left
		{ {-1, -1,  1}, {  o1, o12 } },  // up right
		{ {-1, -1,  0}, {  o1,  o0 } },  // down right
		{ {-1, -1,  0}, {  o1,  o0 } },  // down right
		{ { 1, -1,  0}, { o23,  o0 } },  // down left
		{ { 1, -1,  1}, { o23, o12 } },  // up left
	};

	secondary_light_pass_cubemap_.unwrap_geometry.VertexData(
		unwrap_geometry,
		sizeof(unwrap_geometry),
		sizeof(CubemapGeometryVertex) );
	secondary_light_pass_cubemap_.unwrap_geometry.VertexAttribPointer( 0, 3, GL_FLOAT, false, 0 );
	secondary_light_pass_cubemap_.unwrap_geometry.VertexAttribPointer( 1, 2, GL_FLOAT, false, sizeof(float) * 3 );
	secondary_light_pass_cubemap_.unwrap_geometry.SetPrimitiveType( GL_TRIANGLES );

	secondary_light_pass_cubemap_.unwrap_framebuffer=
		r_Framebuffer(
			{ r_Texture::PixelFormat::RGBA32F }, // one floating point texture
			r_Texture::PixelFormat::Unknown, // no depth buffer,
			secondary_light_pass_cubemap_.size * 3,
			secondary_light_pass_cubemap_.size );

	secondary_light_pass_cubemap_.write_shader.ShaderSource(
		rLoadShader( "secondary_light_pass_write_f.glsl", g_glsl_version ),
		rLoadShader( "secondary_light_pass_write_v.glsl", g_glsl_version ),
		rLoadShader( "secondary_light_pass_write_g.glsl", g_glsl_version ));
	secondary_light_pass_cubemap_.write_shader.SetAttribLocation( "tex_coord", 0 );
	secondary_light_pass_cubemap_.write_shader.Create();
}

void plb_LightmapsBuilder::SecondaryLightPass( const m_Vec3& pos, const m_Vec3& normal )
{
	glViewport( 0, 0, secondary_light_pass_cubemap_.size, secondary_light_pass_cubemap_.size );
	glBindFramebuffer( GL_FRAMEBUFFER, secondary_light_pass_cubemap_.fbo_id );
	glClearColor ( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );

	glEnable(GL_CLIP_DISTANCE0);

	// matrices generation
	m_Mat4 final_matrices[6];
	GenCubemapMatrices( pos, normal, final_matrices );

	// bind lightmap texture
	glActiveTexture( GL_TEXTURE0 + 0 );
	glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.tex_id );

	// bind cubemap texture
	unsigned int arrays_bindings_unit= 3;
	textures_manager_->BindTextureArrays(arrays_bindings_unit);
	int textures_uniform[32];
	for( unsigned int i= 0; i< textures_manager_->ArraysCount(); i++ )
		textures_uniform[i]= arrays_bindings_unit + i;

	const auto bind_and_set_uniforms=
	[&]( r_GLSLProgram& shader )
	{
		shader.Bind();
		shader.Uniform( "textures", textures_uniform, textures_manager_->ArraysCount() );
		shader.Uniform( "lightmap", int(0) );
		shader.Uniform( "view_matrices", final_matrices, 6 );
		shader.Uniform( "clip_plane", normal.x, normal.y, normal.z, -(normal * pos) );
	};

	bind_and_set_uniforms( secondary_light_pass_shader_ );
	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::WorldCommon );

	// Alpha-tested polygons (include alpha-tested luminocity polygons
	glDisable( GL_CULL_FACE );
	bind_and_set_uniforms( secondary_light_pass_shader_alphatested_shader_ );
	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::AlphaShadow );
	glEnable( GL_CULL_FACE );


	// Luminocity polygons.
	// Luminocity polygons already drawn, draw it again, but with different texture and shader.
	// Add luminocity light to diffuse surface light.

	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ONE );
	glDepthFunc( GL_EQUAL );

	bind_and_set_uniforms( secondary_light_pass_shader_luminocity_shader_ );
	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::Luminous );

	glDisable( GL_BLEND );
	glDepthFunc( GL_LESS );

	// Draw sky polygons as normal polygons, but with luminocity sahader
	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::Sky );

	glDisable(GL_CLIP_DISTANCE0);

	// Unwrap
	//
	r_Framebuffer::BindScreenFramebuffer();

	glDisable( GL_CULL_FACE );

	secondary_light_pass_cubemap_.unwrap_framebuffer.Bind();
	glClearColor( 1, 0, 1, 0 );
	glClear( GL_COLOR_BUFFER_BIT );

	glActiveTexture( GL_TEXTURE0 + 0 );
	glBindTexture( GL_TEXTURE_CUBE_MAP, secondary_light_pass_cubemap_.tex_id );
	glActiveTexture( GL_TEXTURE0 + 1 );
	glBindTexture( GL_TEXTURE_CUBE_MAP, secondary_light_pass_cubemap_.direction_multipler_tex_id );

	secondary_light_pass_cubemap_.unwrap_shader.Bind();
	secondary_light_pass_cubemap_.unwrap_shader.Uniform( "cubemap", int(0) );
	secondary_light_pass_cubemap_.unwrap_shader.Uniform( "cubemap_multiplier", int(1) );

	secondary_light_pass_cubemap_.unwrap_geometry.Draw();

	glEnable( GL_CULL_FACE );

	r_Framebuffer::BindScreenFramebuffer();

	// Build Mips.
	// TODO - maybe, manually downsample?
	r_Texture& tex= secondary_light_pass_cubemap_.unwrap_framebuffer.GetTextures().front();
	tex.Bind(0);
	tex.SetFiltration( r_Texture::Filtration::NearestMipmapNearest, r_Texture::Filtration::Nearest );
	tex.BuildMips();
}

void plb_LightmapsBuilder::GenDirectionalLightShadowmap( const m_Mat4& shadow_mat )
{
	directional_light_shadowmap_.Bind();

	// Mark all in shadow
	glClearDepth( 0.0f );
	glClear( GL_DEPTH_BUFFER_BIT );
	glClearDepth( 1.0f );

	// Mark sky polygons
	glCullFace( GL_FRONT );
	//glDisable( GL_CULL_FACE );
	glDepthFunc( GL_ALWAYS );

	directional_light_sky_mark_shader_.Bind();
	directional_light_sky_mark_shader_.Uniform( "view_matrix", shadow_mat );

	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::Sky );

	glDepthFunc( GL_LESS );
	glCullFace( GL_BACK );
	//glEnable( GL_CULL_FACE );

	// Draw world polygons
	glDisable( GL_CULL_FACE );

	// Regular geometry
	shadowmap_shader_.Bind();
	shadowmap_shader_.Uniform( "view_matrix", shadow_mat );

	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::WorldCommon );

	// Alpha-tested geometry
	shadowmap_alphatested_shader_.Bind();
	shadowmap_alphatested_shader_.Uniform( "view_matrix", shadow_mat );

	int textures_uniform[32];
	const unsigned int arrays_bindings_unit= 3;
	textures_manager_->BindTextureArrays( arrays_bindings_unit );
	for( unsigned int i= 0; i< textures_manager_->ArraysCount(); i++ )
		textures_uniform[i]= arrays_bindings_unit + i;
	shadowmap_alphatested_shader_.Uniform( "textures", textures_uniform, textures_manager_->ArraysCount() );

	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::AlphaShadow );

	glEnable( GL_CULL_FACE );

	r_Framebuffer::BindScreenFramebuffer();
}

void plb_LightmapsBuilder::DirectionalLightPass(
	const plb_DirectionalLight& light,
	const m_Mat4& shadow_mat )
{
	glDisable( GL_CULL_FACE );
	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ONE );

	glViewport( 0, 0, lightmap_atlas_texture_.size[0], lightmap_atlas_texture_.size[1] );
	glBindFramebuffer( GL_FRAMEBUFFER, lightmap_atlas_texture_.fbo_id );

	directional_light_shadowmap_.GetDepthTexture().Bind(0);

	m_Vec3 light_color( float(light.color[0]), float(light.color[1]), float(light.color[2]) );
	light_color*= light.intensity / 255.0f;

	directional_light_pass_shader_.Bind();
	directional_light_pass_shader_.Uniform( "light_dir", m_Vec3(light.direction) );
	directional_light_pass_shader_.Uniform( "light_color", light_color );
	directional_light_pass_shader_.Uniform( "shadowmap", int(0) );
	directional_light_pass_shader_.Uniform( "view_matrix", shadow_mat );

	world_vertex_buffer_->DrawLightmapTexels();

	r_Framebuffer::BindScreenFramebuffer();

	glEnable( GL_CULL_FACE );
	glDisable( GL_BLEND );
}

void plb_LightmapsBuilder::GenConeLightShadowmap( const m_Mat4& shadow_mat )
{
	glDisable( GL_CULL_FACE );

	cone_light_shadowmap_.Bind();
	glClear( GL_DEPTH_BUFFER_BIT );

	// Regular geometry
	shadowmap_shader_.Bind();
	shadowmap_shader_.Uniform( "view_matrix", shadow_mat );

	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::WorldCommon );

	// Alpha-tested geometry
	shadowmap_alphatested_shader_.Bind();
	shadowmap_alphatested_shader_.Uniform( "view_matrix", shadow_mat );

	int textures_uniform[32];
	const unsigned int arrays_bindings_unit= 3;
	textures_manager_->BindTextureArrays( arrays_bindings_unit );
	for( unsigned int i= 0; i< textures_manager_->ArraysCount(); i++ )
		textures_uniform[i]= arrays_bindings_unit + i;
	shadowmap_alphatested_shader_.Uniform( "textures", textures_uniform, textures_manager_->ArraysCount() );

	world_vertex_buffer_->Draw( plb_WorldVertexBuffer::PolygonType::AlphaShadow );

	r_Framebuffer::BindScreenFramebuffer();
	glEnable( GL_CULL_FACE );
}

void plb_LightmapsBuilder::ConeLightPass( const plb_ConeLight& light, const m_Mat4& shadow_mat )
{
	glDisable( GL_CULL_FACE );
	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ONE );

	glViewport( 0, 0, lightmap_atlas_texture_.size[0], lightmap_atlas_texture_.size[1] );
	glBindFramebuffer( GL_FRAMEBUFFER, lightmap_atlas_texture_.fbo_id );

	cone_light_shadowmap_.GetDepthTexture().Bind(0);

	m_Vec3 light_color( float(light.color[0]), float(light.color[1]), float(light.color[2]) );
	light_color*= light.intensity / 255.0f;

	cone_light_pass_shader_.Bind();
	cone_light_pass_shader_.Uniform( "light_pos", m_Vec3(light.pos) );
	cone_light_pass_shader_.Uniform( "light_color", light_color );
	cone_light_pass_shader_.Uniform( "shadowmap", int(0) );
	cone_light_pass_shader_.Uniform( "view_matrix", shadow_mat );

	world_vertex_buffer_->DrawLightmapTexels();

	r_Framebuffer::BindScreenFramebuffer();

	glEnable( GL_CULL_FACE );
	glDisable( GL_BLEND );
}

void plb_LightmapsBuilder::MarkLuminousMaterials()
{
	for( plb_Material& material : level_data_.materials )
	{
		if( material.luminosity > config_.max_luminocity_for_direct_luminous_surfaces_drawing )
			material.split_to_point_lights= true;
	}
}

void plb_LightmapsBuilder::BuildLuminousSurfacesLights()
{
	typedef plb_Rasterizer<float> Rasterizer;

	const unsigned int c_scale_in_rasterizer= 4;
	const float subdivide_inv_size= config_.luminous_surfaces_tessellation_inv_size;

	std::vector<float> rasterizer_data;

	for( const plb_Polygon& poly : level_data_.polygons )
	{
		const plb_Material& material= level_data_.materials[ poly.material_id ];
		if( !( material.luminosity > 0.0f && material.split_to_point_lights ) )
			continue;

		const plb_ImageInfo& image_info= level_data_.textures[ material.light_texture_number ];

		unsigned char average_texture_color[4];
		textures_manager_->GetTextureAverageColor(
			image_info.texture_array_id,
			image_info.texture_layer_id,
			average_texture_color );

		m_Vec3 polygon_projection_basis[2]; // normalized
		for( unsigned int i= 0; i < 2; i++ )
		{
			polygon_projection_basis[i]= m_Vec3( poly.lightmap_basis[i] );
			polygon_projection_basis[i].Normalize();
		}

		m_Mat3 inv_basis;
		plbGetInvLightmapBasisMatrix(
			polygon_projection_basis[0], polygon_projection_basis[1],
			inv_basis );

		// Calculate size of poygon projection.
		m_Vec2 proj_min( +REALLY_MAX_FLOAT, +REALLY_MAX_FLOAT );
		m_Vec2 proj_max( -REALLY_MAX_FLOAT, -REALLY_MAX_FLOAT );
		for( unsigned int v= poly.first_vertex_number; v < poly.first_vertex_number + poly.vertex_count; v++ )
		{
			const m_Vec3 relative_pos=
				m_Vec3( level_data_.vertices[v].pos ) -
				m_Vec3( poly.lightmap_pos );

			const m_Vec2 projection_pos= ( relative_pos * inv_basis ).xy();

			if( projection_pos.x < proj_min.x ) proj_min.x= projection_pos.x;
			if( projection_pos.y < proj_min.y ) proj_min.y= projection_pos.y;
			if( projection_pos.x > proj_max.x ) proj_max.x= projection_pos.x;
			if( projection_pos.y > proj_max.y ) proj_max.y= projection_pos.y;
		} // for vertices

		proj_min*= subdivide_inv_size;
		proj_max*= subdivide_inv_size;
		const m_Vec2 proj_size= proj_max - proj_min;

		// Small polygon - generate single light source.
		if( proj_size.x < 1.5f && proj_size.y < 1.5f )
		{
			bright_luminous_surfaces_lights_.emplace_back();
			plb_SurfaceSampleLight& light= bright_luminous_surfaces_lights_.back();

			light.intensity=
				plb_Constants::inv_pi * material.luminosity *
				plbGetPolygonArea( poly, level_data_.vertices, level_data_.polygons_indeces );

			const m_Vec3 pos=
				plbGetPolygonCenter( poly, level_data_.vertices, level_data_.polygons_indeces );

			for( unsigned int i= 0; i < 3; i++ )
			{
				light.pos[i]= pos.ToArr()[i];
				light.normal[i]= poly.normal[i];
			}

			std::memcpy( light.color, average_texture_color, 3 );

			continue;
		} // small polygon

		// Create rasterizer
		Rasterizer::Buffer rasterizer_buffer;
		unsigned int light_grid_size[2];

		for( unsigned int i= 0; i < 2; i++ )
		{
			light_grid_size[i]= static_cast<unsigned int>( std::ceil( proj_size.ToArr()[i] ) );
			rasterizer_buffer.size[i]= light_grid_size[i] * c_scale_in_rasterizer;

			// Move polygon projection to center of light grid.
			proj_min.ToArr()[i]-= 0.5f * ( float(light_grid_size[i]) - proj_size.ToArr()[i] );
		}

		rasterizer_data.clear();
		rasterizer_data.resize( rasterizer_buffer.size[0] * rasterizer_buffer.size[1], 0.0f );
		rasterizer_buffer.data= rasterizer_data.data();

		Rasterizer rasterizer( rasterizer_buffer );

		// Rasterize polygon triangles.
		for( unsigned int t= 0; t < poly.index_count; t+= 3 )
		{
			m_Vec2 v[3];
			float attrib[3]= { 1.0f, 1.0f, 1.0f };

			const unsigned int* const triangle_indeces= level_data_.polygons_indeces.data() + poly.first_index + t;
			for( unsigned int i= 0; i < 3; i++ )
			{
				const m_Vec3 world_space_vertex=
					m_Vec3( level_data_.vertices[ triangle_indeces[i] ].pos ) -
					m_Vec3( poly.lightmap_pos );

				v[i]= ( world_space_vertex * inv_basis ).xy();
				v[i]*= subdivide_inv_size;
				v[i]-= proj_min;
				v[i]*= float(c_scale_in_rasterizer);
			}

			rasterizer.DrawTriangle( v, attrib );
		} // for polygon triangles

		// Get back data from rasterizer, create final light sources.
		for( unsigned int y= 0; y < light_grid_size[1]; y++ )
		for( unsigned int x= 0; x < light_grid_size[0]; x++ )
		{
			float covered= 0.0f;
			for( unsigned int v= 0; v < c_scale_in_rasterizer; v++ )
			for( unsigned int u= 0; u < c_scale_in_rasterizer; u++ )
			{
				covered+=
					rasterizer_data[
						x * c_scale_in_rasterizer + u +
						( y * c_scale_in_rasterizer + v ) * rasterizer_buffer.size[0] ];
			}
			if( covered < 0.5f )
				continue;

			covered/= float( c_scale_in_rasterizer * c_scale_in_rasterizer );

			bright_luminous_surfaces_lights_.emplace_back();
			plb_SurfaceSampleLight& light= bright_luminous_surfaces_lights_.back();

			light.intensity=
				plb_Constants::inv_pi * material.luminosity *
				covered / ( subdivide_inv_size * subdivide_inv_size );

			const m_Vec3 pos=
				( float(x) + 0.5f + proj_min.x ) / subdivide_inv_size * polygon_projection_basis[0] +
				( float(y) + 0.5f + proj_min.y ) / subdivide_inv_size * polygon_projection_basis[1] +
				m_Vec3( poly.lightmap_pos );

			for( unsigned int i= 0; i < 3; i++ )
			{
				light.pos[i]= pos.ToArr()[i];
				light.normal[i]= poly.normal[i];
			}

			std::memcpy( light.color, average_texture_color, 3 );
		} // for light grid

	} // for polygons


	std::cout << "Bright luminous surfaces lights generated: " <<
		bright_luminous_surfaces_lights_.size() <<
		std::endl;
}

void plb_LightmapsBuilder::BuildLightmapBasises()
{
	float basis_scaler= 1.0f / float( 1 << config_.inv_lightmap_scale_log2 );

	const plb_Vertex* v_p= level_data_.vertices.data();
	for( plb_Polygon& poly : level_data_.polygons )
	{
		float min_uv[2]= { REALLY_MAX_FLOAT, REALLY_MAX_FLOAT };

		for( unsigned int v= poly.first_vertex_number; v< poly.first_vertex_number + poly.vertex_count; v++ )
		{
			float uv[2]= {
				m_Vec3( v_p[v].pos ) * m_Vec3( poly.texture_basis[0] ) + poly.texture_basis[0][3],
				m_Vec3( v_p[v].pos ) * m_Vec3( poly.texture_basis[1] ) + poly.texture_basis[1][3] };
			if( uv[0] < min_uv[0] ) min_uv[0]= uv[0];
			if( uv[1] < min_uv[1] ) min_uv[1]= uv[1];
		}

		m_Vec3 scaled_u_basis(poly.texture_basis[0]);
		scaled_u_basis*= basis_scaler;
		m_Vec3 scaled_v_basis(poly.texture_basis[1]);
		scaled_v_basis*= basis_scaler;
		
		ARR_VEC3_CPY( poly.lightmap_basis[0], scaled_u_basis );
		ARR_VEC3_CPY( poly.lightmap_basis[1], scaled_v_basis );
		poly.lightmap_basis[0][3]= -floorf(min_uv[0]) * basis_scaler;
		poly.lightmap_basis[1][3]= -floorf(min_uv[1]) * basis_scaler;

	}// for polygons
}

/*
UNFINISHED
*/
void plb_LightmapsBuilder::DevideLongPolygons()
{
#if 0
	plb_Vertex* v_p= &*level_data_.vertices.begin();
	unsigned int initial_polygons_count= level_data_.polygons.size();

	for( unsigned int i= 0; i< initial_polygons_count; i++ )
	{
		plb_Polygon* polygon= &level_data_.polygons[i];

		float min_uv[2]= {REALLY_MAX_FLOAT, REALLY_MAX_FLOAT }, max_uv[2]= { -REALLY_MAX_FLOAT, -REALLY_MAX_FLOAT };
		for( unsigned int v= polygon->first_vertex_number; v< polygon->first_vertex_number + polygon->vertex_count; v++ )
		{
			float uv[2]= {
				m_Vec3( polygon->lightmap_basis[0] ) * m_Vec3( v_p[v].pos ) + polygon->lightmap_basis[0][3],
				m_Vec3( polygon->lightmap_basis[1] ) * m_Vec3( v_p[v].pos ) + polygon->lightmap_basis[1][3] };
			if( uv[0] < min_uv[0] ) min_uv[0]= uv[0];
			else if( uv[0] > max_uv[0] ) max_uv[0]= uv[0];
			if( uv[1] < min_uv[1] ) min_uv[1]= uv[1];
			else if( uv[1] > max_uv[1] ) max_uv[1]= uv[1];
		}
		//printf( "uv: min(%f:%f) max(%f:%f)\n", min_uv[0], min_uv[1], max_uv[0], max_uv[1] );
	}// for polygons
#endif
}

void plb_LightmapsBuilder::TransformTexturesCoordinates()
{
	plb_Vertex* v_p= level_data_.vertices.data();
	for( const plb_Polygon& polygon : level_data_.polygons )
	{
		const plb_ImageInfo& img=
			level_data_.textures[ level_data_.materials[ polygon.material_id ].albedo_texture_number ];
		for( unsigned int v= polygon.first_vertex_number; v< polygon.first_vertex_number + polygon.vertex_count; v++ )
		{
			v_p[v].tex_maps[0]= img.texture_array_id;
			v_p[v].tex_maps[1]= img.texture_layer_id;
		}
	}

	if( level_data_.curved_surfaces.size() > 0 )
	{
		v_p= level_data_.curved_surfaces_vertices.data();
		for( const plb_CurvedSurface& curve : level_data_.curved_surfaces )
		{
			const plb_ImageInfo& img=
				level_data_.textures[ level_data_.materials[ curve.material_id ].albedo_texture_number ];
			for( unsigned int v= curve.first_vertex_number; v< curve.first_vertex_number + curve.grid_size[0] * curve.grid_size[1]; v++ )
			{
				v_p[v].tex_maps[0]= img.texture_array_id;
				v_p[v].tex_maps[1]= img.texture_layer_id;
			}
		}
	}

	// Quake1BSP and Quake2BSP stroes unnormalized textures coordinates. Normalize it.
	if(
		config_.source_data_type == plb_Config::SourceDataType::Quake1BSP ||
		config_.source_data_type == plb_Config::SourceDataType::Quake2BSP ||
		config_.source_data_type == plb_Config::SourceDataType::HalfLifeBSP)
	{
		for( const plb_Polygon& polygon : level_data_.polygons )
		{
			const plb_ImageInfo& img=
				level_data_.textures[ level_data_.materials[ polygon.material_id ].albedo_texture_number ];

			const float scale_x= 1.0f / float( img.original_size[0] );
			const float scale_y= 1.0f / float( img.original_size[1] );

			for( unsigned int v= polygon.first_vertex_number; v< polygon.first_vertex_number + polygon.vertex_count; v++ )
			{
				v_p[v].tex_coord[0]*= scale_x;
				v_p[v].tex_coord[1]*= scale_y;
			}
		}
	}
}

void plb_LightmapsBuilder::ClalulateLightmapAtlasCoordinates()
{
	if( config_.lightmap_scale_to_original != 1 )
	{
		const float inv_scale= 1.0f / float(config_.lightmap_scale_to_original);
		for( plb_Polygon& polygon : level_data_.polygons )
		{
			polygon.lightmap_basis[0][0]*= inv_scale;
			polygon.lightmap_basis[0][1]*= inv_scale;
			polygon.lightmap_basis[0][2]*= inv_scale;
			polygon.lightmap_basis[1][0]*= inv_scale;
			polygon.lightmap_basis[1][1]*= inv_scale;
			polygon.lightmap_basis[1][2]*= inv_scale;
		}

		for( plb_CurvedSurface & curve : level_data_.curved_surfaces )
		{
			curve.lightmap_data.size[0]*= config_.lightmap_scale_to_original;
			curve.lightmap_data.size[1]*= config_.lightmap_scale_to_original;
		}
	}

	plb_Vertex* v_p= level_data_.vertices.data();
	for( plb_Polygon& polygon : level_data_.polygons )
	{
		float max_uv[2]= { -REALLY_MAX_FLOAT, -REALLY_MAX_FLOAT };

		m_Mat3 inverse_lightmap_basis;
		plbGetInvLightmapBasisMatrix(
			m_Vec3( polygon.lightmap_basis[0] ),
			m_Vec3( polygon.lightmap_basis[1] ),
			inverse_lightmap_basis );

		for( unsigned int v= polygon.first_vertex_number; v< polygon.first_vertex_number + polygon.vertex_count; v++ )
		{
			const m_Vec3 rel_pos= m_Vec3( v_p[v].pos ) - m_Vec3( polygon.lightmap_pos );
			const m_Vec2 uv= ( rel_pos * inverse_lightmap_basis ).xy();
			if( uv.x > max_uv[0] ) max_uv[0]= uv.x;
			if( uv.y > max_uv[1] ) max_uv[1]= uv.y;
		}
		if( max_uv[0] < 1.0f )
			max_uv[0]= 1.0f;
		if( max_uv[1] < 1.0f )
			max_uv[1]= 1.0f;

		polygon.lightmap_data.size[0]= ((unsigned int)std::ceil( max_uv[0] ) ) + 1;
		polygon.lightmap_data.size[1]= ((unsigned int)std::ceil( max_uv[1] ) ) + 1;

		// pereveracivajem bazis karty osvescenija, tak nado
		if( polygon.lightmap_data.size[0] < polygon.lightmap_data.size[1] )
		{
			float tmp[3];
			std::memcpy( tmp, polygon.lightmap_basis[0], sizeof(float) * 3 );
			std::memcpy( polygon.lightmap_basis[0], polygon.lightmap_basis[1], sizeof(float) * 3 );
			std::memcpy( polygon.lightmap_basis[1], tmp, sizeof(float) * 3 );

			std::swap( polygon.lightmap_data.size[0], polygon.lightmap_data.size[1] );
		}
	}

	if( level_data_.curved_surfaces_vertices.size() > 0 )
	{
		v_p= level_data_.curved_surfaces_vertices.data();
		for( plb_CurvedSurface& curve : level_data_.curved_surfaces )
		{
			if( curve.lightmap_data.size[0] < curve.lightmap_data.size[1] )
			{
				unsigned short tmp= curve.lightmap_data.size[0];
				curve.lightmap_data.size[0]= curve.lightmap_data.size[1];
				curve.lightmap_data.size[1]= tmp;

				for( unsigned int v= curve.first_vertex_number;
					v< curve.first_vertex_number + curve.grid_size[0] * curve.grid_size[1]; v++ )
				{
					std::swap( v_p[v].lightmap_coord[0], v_p[v].lightmap_coord[1] );
				}
			}
		}
	}

	std::vector<plb_SurfaceLightmapData*> sorted_lightmaps(
		level_data_.polygons.size() + level_data_.curved_surfaces.size() );

	for( unsigned int i= 0; i< level_data_.polygons.size(); i++ )
		sorted_lightmaps[i]= &level_data_.polygons[i].lightmap_data;
	
	for( unsigned int i= 0; i< level_data_.curved_surfaces.size(); i++ )
		sorted_lightmaps[ i + level_data_.polygons.size() ]=
			&level_data_.curved_surfaces[i].lightmap_data;

	std::sort(
		sorted_lightmaps.begin(),
		sorted_lightmaps.end(),
		[]( const plb_SurfaceLightmapData* l0, const plb_SurfaceLightmapData* l1 )
		{
			return l0->size[1] > l1->size[1];
		} );

	/*
	place lightmaps into atlases
	*/
	const unsigned int lightmaps_offset= config_.secondary_lightmap_scaler;
	const unsigned int lightmap_size[2]=
	{ // Cut pixels, which not exist in secondary lightmap
		config_.lightmaps_atlas_size[0] / config_.secondary_lightmap_scaler * config_.secondary_lightmap_scaler,
		config_.lightmaps_atlas_size[1] / config_.secondary_lightmap_scaler * config_.secondary_lightmap_scaler,
	};
	unsigned int current_lightmap_atlas_id= 0;
	unsigned int current_column_x= lightmaps_offset;
	unsigned int current_column_y= lightmaps_offset;
	unsigned int current_column_height= sorted_lightmaps.front()->size[1];

	for( plb_SurfaceLightmapData* const lightmap : sorted_lightmaps )
	{
		const unsigned int width_in_atlas=
			( lightmap->size[0] + config_.secondary_lightmap_scaler - 1 ) /
			config_.secondary_lightmap_scaler * config_.secondary_lightmap_scaler;

		if( current_column_x + width_in_atlas + lightmaps_offset >= lightmap_size[0] )
		{
			const unsigned int height_in_atlas=
				( lightmap->size[1] + config_.secondary_lightmap_scaler - 1 ) /
				config_.secondary_lightmap_scaler * config_.secondary_lightmap_scaler;

			current_column_x= lightmaps_offset;
			current_column_y+= current_column_height + lightmaps_offset;
			current_column_height= height_in_atlas;

			if( current_column_height + current_column_y + lightmaps_offset >= lightmap_size[1] )
			{
				current_lightmap_atlas_id++;
				current_column_y= lightmaps_offset;
			}
		}
		
		lightmap->coord[0]= current_column_x;
		lightmap->coord[1]= current_column_y;
		lightmap->atlas_id= current_lightmap_atlas_id;

		current_column_x+= width_in_atlas + lightmaps_offset;
	}// for polygons

	lightmap_atlas_texture_.size[0]= config_.lightmaps_atlas_size[0];
	lightmap_atlas_texture_.size[1]= config_.lightmaps_atlas_size[1];
	lightmap_atlas_texture_.size[2]= current_lightmap_atlas_id+1;
}


void plb_LightmapsBuilder::CreateLightmapBuffers()
{
	unsigned int lightmap_size[2]= { config_.lightmaps_atlas_size[0], config_.lightmaps_atlas_size[1] };

	unsigned char* lightmap_data= new unsigned char[ lightmap_size[0] * lightmap_size[1] * 4 ];

	lightmap_atlas_texture_.secondary_lightmap_size[0]=
		lightmap_size[0] / config_.secondary_lightmap_scaler;
	lightmap_atlas_texture_.secondary_lightmap_size[1]=
		lightmap_size[1] / config_.secondary_lightmap_scaler;

	//secondary ambient lightmap textures
	for( unsigned int i= 0; i< 1; i++ )
	{
		glGenTextures( 1, &lightmap_atlas_texture_.secondary_tex_id[i] );
		glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.secondary_tex_id[i] );
		glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32F,
			lightmap_atlas_texture_.secondary_lightmap_size[0],lightmap_atlas_texture_.secondary_lightmap_size[1],
			lightmap_atlas_texture_.size[2],
			0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	}

	// main lightmaps atlas
	glGenTextures( 1, &lightmap_atlas_texture_.tex_id );
	glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.tex_id );
	glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32F, lightmap_size[0], lightmap_size[1], lightmap_atlas_texture_.size[2],
		0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

	// test 8-bit texture for separate colors for each polygon
	glGenTextures( 1, &lightmap_atlas_texture_.colored_test_tex_id );
	glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.colored_test_tex_id );
	glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_R3_G3_B2, lightmap_size[0], lightmap_size[1], lightmap_atlas_texture_.size[2],
		0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

	{
		glGenFramebuffers( 1, &lightmap_atlas_texture_.fbo_id );
		glBindFramebuffer( GL_FRAMEBUFFER, lightmap_atlas_texture_.fbo_id );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , lightmap_atlas_texture_.tex_id, 0 );
		GLuint ca= GL_COLOR_ATTACHMENT0;
		glDrawBuffers( 1, &ca );

		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	for( unsigned int i= 0; i< lightmap_atlas_texture_.size[2]; i++ )
	{
		memset(lightmap_data, 0, lightmap_size[0] * lightmap_size[1] * 4 );
		for( unsigned int p= 0; p< level_data_.polygons.size(); p++ )
		{
			if( level_data_.polygons[p].lightmap_data.atlas_id == i )
			{
				unsigned char color[4]= { rand(), rand(), rand(), rand() };
				int i_c= *((int*)color);

				plb_Polygon* poly= &level_data_.polygons[p];
				for( unsigned int y= poly->lightmap_data.coord[1];
						y< poly->lightmap_data.coord[1] + poly->lightmap_data.size[1]; y++ )
					for( unsigned int x= poly->lightmap_data.coord[0];
						x< poly->lightmap_data.coord[0] + poly->lightmap_data.size[0]; x++ )
						((int*)lightmap_data)[ x + y * lightmap_size[0] ] = i_c;
			}
		}
		glTexSubImage3D( GL_TEXTURE_2D_ARRAY, 0,
			0, 0, i,
			lightmap_size[0], lightmap_size[1], 1,
			GL_RGBA, GL_UNSIGNED_BYTE, lightmap_data );

	}// for atlases
	delete[] lightmap_data;

	float inv_lightmap_size[2]= 
	{
		1.0f / float(lightmap_size[0]),
		1.0f / float(lightmap_size[1]),
	};

	plb_Vertex* v_p= level_data_.vertices.data();
	for( plb_Polygon& poly : level_data_.polygons )
	{
		m_Mat3 inverse_lightmap_basis;
		plbGetInvLightmapBasisMatrix(
			m_Vec3( poly.lightmap_basis[0] ),
			m_Vec3( poly.lightmap_basis[1] ),
			inverse_lightmap_basis );

		for( unsigned int v= poly.first_vertex_number; v< poly.first_vertex_number + poly.vertex_count; v++ )
		{
			const m_Vec3 rel_pos= m_Vec3( v_p[v].pos ) - m_Vec3( poly.lightmap_pos );

			const m_Vec2 uv= ( rel_pos * inverse_lightmap_basis ).xy();
			v_p[v].lightmap_coord[0]= uv.x;
			v_p[v].lightmap_coord[0]+= float(poly.lightmap_data.coord[0]);
			v_p[v].lightmap_coord[0]*= inv_lightmap_size[0];

			v_p[v].lightmap_coord[1]= uv.y;
			v_p[v].lightmap_coord[1]+= float(poly.lightmap_data.coord[1]);
			v_p[v].lightmap_coord[1]*= inv_lightmap_size[1];

			v_p[v].tex_maps[2]= poly.lightmap_data.atlas_id;
		}
	}// for polygons

	if( level_data_.curved_surfaces_vertices.size() > 0 )
	{
		v_p= level_data_.curved_surfaces_vertices.data();
		for( const plb_CurvedSurface& curve : level_data_.curved_surfaces )
		{
			for( unsigned int v= curve.first_vertex_number;
				v< curve.first_vertex_number + curve.grid_size[0] * curve.grid_size[1]; v++ )
			{
				v_p[v].lightmap_coord[0]= v_p[v].lightmap_coord[0] * float(curve.lightmap_data.size[0]) + float(curve.lightmap_data.coord[0]);
				v_p[v].lightmap_coord[0]*= inv_lightmap_size[0];

				v_p[v].lightmap_coord[1]= v_p[v].lightmap_coord[1] * float(curve.lightmap_data.size[1]) + float(curve.lightmap_data.coord[1]);
				v_p[v].lightmap_coord[1]*= inv_lightmap_size[1];

				v_p[v].tex_maps[2]= curve.lightmap_data.atlas_id;
			}
		}// for curves
	}
}


void plb_LightmapsBuilder::FillBorderLightmapTexels()
{
	float* lightmap_data= new float[
		lightmap_atlas_texture_.size[0] *
		lightmap_atlas_texture_.size[1] *
		lightmap_atlas_texture_.size[2] * 4 ];

	const unsigned int max_poly_lightmap_heigth= 512;
	float* tmp_poly_lightmap_data= new float[ max_poly_lightmap_heigth * lightmap_atlas_texture_.size[0] * 4 ];
	
	glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.tex_id );
	glGetTexImage( GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_FLOAT, lightmap_data );


	static const float sum_table[]= { 0.0f, 1.0f, 0.5f, 0.3333333333333333f, 0.25f };
	static const int vec_table[]= { 1,0, -1,0, 0,1, 0,-1 };
	const float eps= 1e-5f;

	for( plb_Polygon& p : level_data_.polygons )
	{
		for( unsigned int i= 0; i< 3; i++ )
		{
			float* texels= lightmap_data + 4 * lightmap_atlas_texture_.size[0] * lightmap_atlas_texture_.size[1] *
				p.lightmap_data.atlas_id;
			float* texels_src= tmp_poly_lightmap_data;

			for( unsigned int y= p.lightmap_data.coord[1], y_end= p.lightmap_data.size[1] + p.lightmap_data.coord[1];
				y< y_end; y++ )
			{
				unsigned int dy= y - p.lightmap_data.coord[1];
				unsigned int k_src= (p.lightmap_data.coord[0] + y * lightmap_atlas_texture_.size[0])<<2;
				unsigned int k_dst= (p.lightmap_data.coord[0] + dy * lightmap_atlas_texture_.size[0])<<2;
				for( unsigned int x= p.lightmap_data.coord[0], x_end= p.lightmap_data.size[0] + p.lightmap_data.coord[0];
					x< x_end; x++, k_src+=4, k_dst+=4 )
				{
					texels_src[k_dst  ]= texels[k_src  ];
					texels_src[k_dst+1]= texels[k_src+1];
					texels_src[k_dst+2]= texels[k_src+2];
					texels_src[k_dst+3]= texels[k_src+3];
				}
			}// copy old data to tmp buffer

			for( unsigned int y= p.lightmap_data.coord[1], y_end= p.lightmap_data.size[1] + p.lightmap_data.coord[1];
				y< y_end; y++ )
			{
				unsigned int dy= y - p.lightmap_data.coord[1];
				unsigned int k_dst= (p.lightmap_data.coord[0] + y * lightmap_atlas_texture_.size[0])<<2;
				unsigned int k_src= (p.lightmap_data.coord[0] + dy * lightmap_atlas_texture_.size[0])<<2;
				for( unsigned int x= p.lightmap_data.coord[0], x_end= p.lightmap_data.size[0] + p.lightmap_data.coord[0];
					x< x_end; x++, k_dst+= 4, k_src+= 4 )
				{
					if( texels_src[k_src+3] < eps ) // empty texel, becouse alpha is zero
					{
						unsigned int notzero_count= 0;
						float sum[]= {0.0f, 0.0f, 0.0f};
						for( unsigned int v= 0; v< 4; v++ )
						{
							int new_x= x + vec_table[v*2];
							int new_y= y + vec_table[v*2+1];
							if( new_x >= p.lightmap_data.coord[0] && new_x < ((int)x_end) )
								if( new_y >= p.lightmap_data.coord[1] && new_y < ((int)y_end) )
								{
									unsigned int new_k= (new_x + (new_y-p.lightmap_data.coord[1]) * lightmap_atlas_texture_.size[0])<<2;
									if( texels_src[new_k+3] > eps ) // if nearest texel alpha is not zero
									{
										sum[0]+= texels_src[new_k  ];
										sum[1]+= texels_src[new_k+1];
										sum[2]+= texels_src[new_k+2];
										notzero_count++;
									}
								}
						}// for nearest texels
						if( notzero_count > 0 )
						{
							texels[k_dst  ]= sum[0] * sum_table[notzero_count];
							texels[k_dst+1]= sum[1] * sum_table[notzero_count];
							texels[k_dst+2]= sum[2] * sum_table[notzero_count];
							texels[k_dst+3]= 1.0f; // mark as not null
						}
					}
				}// for x
			}// for y
		}// fill iterations
	}// for polygons

	delete[] tmp_poly_lightmap_data;

	glTexSubImage3D( GL_TEXTURE_2D_ARRAY, 0,
		0, 0, 0,
		lightmap_atlas_texture_.size[0], lightmap_atlas_texture_.size[1], lightmap_atlas_texture_.size[2],
		GL_RGBA, GL_FLOAT, lightmap_data );
	delete[] lightmap_data;
}


void plb_LightmapsBuilder::CalculateLevelBoundingBox()
{
	const float inf= 1e24f;
	m_Vec3 l_min( inf, inf, inf );
	m_Vec3 l_max( -inf, -inf, -inf );

	for( const plb_Vertex& v : level_data_.vertices )
	{
		for( unsigned int i= 0; i< 3; i++ )
		{
			if( v.pos[i] < l_min.ToArr()[i] ) l_min.ToArr()[i]= v.pos[i];
			else if( v.pos[i] > l_max.ToArr()[i] ) l_max.ToArr()[i]= v.pos[i];
		}
	}
	level_bounding_box_.min= l_min;
	level_bounding_box_.max= l_max;
}

m_Vec3 plb_LightmapsBuilder::CorrectSecondaryLightSample( const m_Vec3& pos, const plb_Polygon& poly )
{
	const float c_up_eps= 1.0f / 16.0f;
	const float nudge_world_space_eps= 1.0f / 128.0f;

	const m_Vec2 c_check_rays[8]=
	{
		m_Vec2(  0.0f, 1.0f ), m_Vec2(  0.0f, -1.0f ),
		m_Vec2(  1.0f, 0.0f ), m_Vec2( -1.0f,  0.0f ),
		m_Vec2(  1.0f, 1.0f ), m_Vec2(  1.0f, -1.0f ),
		m_Vec2( -1.0f, 1.0f ), m_Vec2( -1.0f, -1.0f ),
	};

	m_Vec3 result_pos= pos;
	float max_move_square_distance= 0.0f;

	const auto set_result_candidate=
		[&]( const m_Vec3& moved_pos )
		{
			const float square_distance= ( moved_pos - pos ).SquareLength();
			if( square_distance > max_move_square_distance )
			{
				max_move_square_distance= square_distance;
				result_pos= moved_pos;
			}
		};

	const m_Vec3 up_shift= c_up_eps * m_Vec3( poly.normal );
	const m_Vec3 pos_up_shifted= pos + up_shift;

	const float lightmap_scale= float(config_.secondary_lightmap_scaler);

	for( const m_Vec2& ray : c_check_rays )
	{
		constexpr unsigned int c_max_intersections= 8;
		plb_Tracer::TraceResult trace_result[ c_max_intersections ];

		const m_Vec3 ray_world_space=
			lightmap_scale * ray.x * m_Vec3(poly.lightmap_basis[0]) +
			lightmap_scale * ray.y * m_Vec3(poly.lightmap_basis[1]);

		const m_Vec3 normalized_ray_world_space= ray_world_space / ray_world_space.Length();

		// Move sample point just a bit, because we need prevent cases,
		// when sample point is exactly on polygon plane.
		const m_Vec3 nudge_vec= normalized_ray_world_space * nudge_world_space_eps;

		const unsigned int intersection_count=
			std::min(
				c_max_intersections,
				tracer_->Trace(
					pos_up_shifted - nudge_vec,
					pos_up_shifted + ray_world_space,
					trace_result, c_max_intersections ) );

		if( intersection_count == 0 )
			continue;

		// Sort from near to far
		std::sort(
			trace_result, trace_result + intersection_count,
			[&]( const plb_Tracer::TraceResult& l, const plb_Tracer::TraceResult& r )
			{
				return
					( l.pos - pos_up_shifted ).SquareLength() <=
					( r.pos - pos_up_shifted ).SquareLength();
			} );

		// First intersection is back face
		if( trace_result[0].normal * ray_world_space > 0.0f )
		{
			// Search first front face after back face
			unsigned int i= 0;
			while(
				i < intersection_count &&
				trace_result[i].normal * ray_world_space > 0.0f )
			{
				i++;
			}
			i--;

			// Move sample point beyound intersection plane.
			const m_Vec3 moved_pos=
				plbProjectPointToPlane( pos, trace_result[i].pos, trace_result[i].normal ) +
				trace_result[i].normal * g_cubemaps_min_clip_distance;
			set_result_candidate( moved_pos );
		}
		else // Front face
		{
			const m_Vec3 projected_pos= plbProjectPointToPlane( pos, trace_result[0].pos, trace_result[0].normal );

			// Try move sample point from intersection plane just a bit.
			const float dist= ( pos - projected_pos ) * trace_result[0].normal;
			if( dist < g_cubemaps_min_clip_distance )
			{
				const m_Vec3 moved_pos=
					projected_pos +
					trace_result[0].normal * g_cubemaps_min_clip_distance;
				set_result_candidate( moved_pos );
			}
		}
	} // for rays

	return result_pos;
}
