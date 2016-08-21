#include <algorithm>
#include <cstring>

#include <shaders_loading.hpp>

#include "lightmaps_builder.hpp"

#include "curves.hpp"
#include "q3_bsp_loader.hpp"

#define VEC3_CPY(dst,src) (dst)[0]= (src)[0]; (dst)[1]= (src)[1]; (dst)[2]= (src)[2];
#define ARR_VEC3_CPY(dst,vec) dst[0]= vec.x; dst[1]= vec.y; dst[2]= vec.z;
#define REALLY_MAX_FLOAT 1e24f

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

static const r_GLSLVersion g_glsl_version( r_GLSLVersion::KnowmNumbers::v430 );

static const float g_pi= 3.1415926535f;

static void TriangulatePolygons(
	const plb_Polygons& in_polygons,
	unsigned int base_vertex,
	std::vector<unsigned int>& out_indeces )
{
	unsigned int triangle_count= 0;
	for( unsigned int i= 0; i< in_polygons.size(); i++ )
		triangle_count+= in_polygons[i].vertex_count - 2;

	out_indeces.resize( out_indeces.size() + triangle_count * 3 );

	unsigned int* i_p= out_indeces.data() + out_indeces.size() - triangle_count * 3;

	for( const plb_Polygon& poly : in_polygons )
		for( unsigned int j= 0; j< poly.vertex_count - 2; j++ )
		{
			i_p[0]= base_vertex + poly.first_vertex_number;
			i_p[1]= base_vertex + poly.first_vertex_number + j + 1;
			i_p[2]= base_vertex + poly.first_vertex_number + j + 2;
			i_p+= 3;
		}
}

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


static void GenCubemapMatrices( const m_Vec3& pos, m_Mat4* out_matrices, float y_rotation= 0.0f )
{
	m_Mat4 perspective, shift, y_rot;
	perspective.PerspectiveProjection( 1.0f, g_pi * 0.5f, 1.0f / 32.0f, 256.0f );
	y_rot.RotateY( y_rotation );
	shift.Translate( -pos );

	m_Mat4 tmp; tmp.RotateZ( g_pi );
	out_matrices[0].RotateY( g_pi * 0.5f );
	out_matrices[1].RotateY( -g_pi* 0.5f );
	out_matrices[2].RotateX( -g_pi * 0.5f );
	out_matrices[2]*= tmp;
	out_matrices[3].RotateX( g_pi * 0.5f );
	out_matrices[3]*= tmp;
	out_matrices[4].RotateY( -g_pi );
	out_matrices[5].Identity();

	for( unsigned int i= 0; i< 6; i++ )
		out_matrices[i]= shift * y_rot * out_matrices[i] * perspective;
}

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
	const float* control_vectors[]=
	{
		side_vectors + side_num * 12,
		side_vectors + side_num * 12 + 3,
		side_vectors + side_num * 12 + 6,
		side_vectors + side_num * 12 + 9
	};

	float dkx= 1.0f / float(size);
	for( unsigned int y= 0; y< size; y++ )
	{
		float yk= float(y) / float(size);
		float yk1= 1.0f - yk;
		m_Vec3 vy[2];
		vy[0]= m_Vec3( control_vectors[0] ) * yk1 + m_Vec3( control_vectors[2] ) * yk;
		vy[1]= m_Vec3( control_vectors[1] ) * yk1 + m_Vec3( control_vectors[3] ) * yk;

		float xk= 0.0f, xk1= 1.0f;
		for( unsigned int x= 0; x< size; x++, xk+= dkx, xk1-= dkx )
		{
			m_Vec3 vec= vy[0] * xk1 + vy[1] * xk;
			m_Vec3 side_vec( xk * 2.0f - 1.0f, yk * 2.0f - 1.0f, 1.0f );
			float f= 255.0f * vec.z / ( vec.Length() * side_vec.Length() );
			
			int fi= int(f);
			if( fi > 255 ) fi= 255;
			else if( fi < 0 ) fi= 0;
			out_data[ x + y * size ]= fi;
		}// for x
	}// for y
}

static void SetupLevelVertexAttributes( r_GLSLProgram& shader )
{
	shader.SetAttribLocation( "pos", Attrib::Pos );
	shader.SetAttribLocation( "tex_coord", Attrib::TexCoord );
	shader.SetAttribLocation( "lightmap_coord", Attrib::LightmapCoord );
	shader.SetAttribLocation( "normal", Attrib::Normal );
	shader.SetAttribLocation( "tex_maps", Attrib::TexMaps );
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

static void CreateDirectionalLightMatrix(
	const plb_DirectionalLight& light,
	const m_Vec3& bb_min,
	const m_Vec3& bb_max,
	m_Mat4& out_mat )
{
	m_Mat4 projection, rotation, shift;

	m_Vec3 dir( light.direction );
	dir.Normalize();

	rotation.RotateX( -g_pi * 0.5f );

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

	const float c_sin_eps= 0.99f;

		 if( light.direction[1] >=  c_sin_eps )
		rotate.RotateX(  g_pi * 0.5f );
	else if( light.direction[1] <= -c_sin_eps )
		rotate.RotateX( -g_pi * 0.5f );
	else
	{
		m_Mat4 rotate_x, rotate_y;
		const float angle_x= std::asin( light.direction[1] );
		const float angle_y= std::atan2( light.direction[0], light.direction[2] );

		rotate_x.RotateX( angle_x);
		rotate_y.RotateY(-angle_y);

		rotate= rotate_y * rotate_x;
	}

	perspective.PerspectiveProjection( 1.0f, 2.0f * light.angle, 0.1f, 256.0f );

	out_mat= translate * rotate * perspective;
}

plb_LightmapsBuilder::plb_LightmapsBuilder( const char* file_name, const plb_Config& config )
	:config_( config )
{
	LoadQ3Bsp( file_name , &level_data_ );
	textures_manager_.reset( new plb_TexturesManager( config_, level_data_.textures ) );

	ClalulateLightmapAtlasCoordinates();
	CreateLightmapBuffers();
	TransformTexturesCoordinates();
	CalculateLevelBoundingBox();

	{ // create wold vbo
		plb_Vertices combined_vertices( level_data_.vertices );
		plb_Normals normals;
		std::vector<unsigned int> index_buffer;

		TriangulatePolygons( level_data_.polygons, 0, index_buffer );
		GenPolygonsVerticesNormals( level_data_.polygons, level_data_.vertices, normals );

		if( level_data_.curved_surfaces.size() > 0 )
			GenCurvesMeshes( level_data_.curved_surfaces, level_data_.curved_surfaces_vertices,
				combined_vertices, index_buffer, normals );

		polygons_vbo_.VertexData(
			combined_vertices.data(),
			combined_vertices.size() * sizeof(plb_Vertex), sizeof(plb_Vertex) );

		polygons_vbo_.IndexData( index_buffer.data(), index_buffer.size() * sizeof(unsigned int), GL_UNSIGNED_INT, GL_TRIANGLES );
		
		plb_Vertex v; unsigned int offset;
		offset= ((char*)v.pos) - ((char*)&v);
		polygons_vbo_.VertexAttribPointer( Attrib::Pos, 3, GL_FLOAT, false, offset );
		offset= ((char*)v.tex_coord) - ((char*)&v);
		polygons_vbo_.VertexAttribPointer( Attrib::TexCoord, 2, GL_FLOAT, false, offset );
		offset= ((char*)v.lightmap_coord) - ((char*)&v);
		polygons_vbo_.VertexAttribPointer( Attrib::LightmapCoord, 2, GL_FLOAT, false, offset );
		offset= ((char*)&v.tex_maps[0]) - ((char*)&v);
		polygons_vbo_.VertexAttribPointerInt( Attrib::TexMaps, 3, GL_UNSIGNED_BYTE, offset );

		glGenBuffers( 1, &polygon_vbo_vertex_normals_vbo_ );
		glBindBuffer( GL_ARRAY_BUFFER, polygon_vbo_vertex_normals_vbo_ );
		glBufferData( GL_ARRAY_BUFFER, normals.size() * sizeof(plb_Normal), normals.data(), GL_STATIC_DRAW );

		glEnableVertexAttribArray( Attrib::Normal );
		glVertexAttribPointer( Attrib::Normal, 3, GL_BYTE, true, sizeof(plb_Normal), NULL );
	}// create world VBO

	polygons_preview_shader_.ShaderSource(
		rLoadShader( "preview_f.glsl", g_glsl_version),
		rLoadShader( "preview_v.glsl", g_glsl_version) );
	SetupLevelVertexAttributes(polygons_preview_shader_);
	polygons_preview_shader_.Create();

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

	{ // Add test light, like in q3dm6
		level_data_.directional_lights.emplace_back();
		plb_DirectionalLight& dl= level_data_.directional_lights.back();

		dl.color[0]= 128; dl.color[1]= 153; dl.color[2]= 192;
		dl.intensity= 150.0f / 8.0f;

		const float c_to_rad= g_pi / 180.0f;
		const float c_elevation= 60.0f * c_to_rad;
		const float c_degrees= 30.0f * c_to_rad;

		dl.direction[1]= std::sin( c_elevation );
		dl.direction[0]= std::cos( c_elevation ) * std::cos( c_degrees );
		dl.direction[2]= std::cos( c_elevation ) * std::sin( c_degrees );
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

	FillBorderLightmapTexels();

	GenSecondaryLightPassCubemap();
}

plb_LightmapsBuilder::~plb_LightmapsBuilder()
{
}

void plb_LightmapsBuilder::DrawPreview( const m_Mat4& view_matrix, const m_Vec3& cam_pos, const m_Vec3& cam_dir )
{
	r_Framebuffer::BindScreenFramebuffer();

	glClearColor ( 0.1f, 0.05f, 0.1f, 0.0f );
	glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glActiveTexture( GL_TEXTURE0 + 0 );
	glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.tex_id );
	glActiveTexture( GL_TEXTURE0 + 1 );
	glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.colored_test_tex_id );
	glActiveTexture( GL_TEXTURE0 + 2 );
	glBindTexture( GL_TEXTURE_CUBE_MAP, point_light_shadowmap_cubemap_.depth_tex_id );

	glActiveTexture( GL_TEXTURE0 + 0 );

	polygons_preview_shader_.Bind();
	polygons_preview_shader_.Uniform( "view_matrix", view_matrix );
	polygons_preview_shader_.Uniform( "lightmap", int(0) );
	polygons_preview_shader_.Uniform( "lightmap_test", int(1) );
	polygons_preview_shader_.Uniform( "cubemap", int(2) );

	unsigned int arrays_bindings_unit= 3;
	textures_manager_->BindTextureArrays(arrays_bindings_unit);
	int textures_uniform[32];
	for( unsigned int i= 0; i< textures_manager_->ArraysCount(); i++ )
		textures_uniform[i]= arrays_bindings_unit + i;
	polygons_preview_shader_.Uniform( "textures", textures_uniform, textures_manager_->ArraysCount() );

	polygons_vbo_.Draw();


	// Debug secondary light pass
	SecondaryLightPass( cam_pos, cam_dir );

	glActiveTexture( GL_TEXTURE0 + 0 );
	glBindTexture( GL_TEXTURE_CUBE_MAP, secondary_light_pass_cubemap_.tex_id );
	glActiveTexture( GL_TEXTURE0 + 1 );
	glBindTexture( GL_TEXTURE_CUBE_MAP, secondary_light_pass_cubemap_.direction_multipler_tex_id );

	texture_show_shader_.Bind();
	texture_show_shader_.Uniform( "cubemap", int(0) );
	texture_show_shader_.Uniform( "cubemap_multipler", int(1) );
	cubemap_show_buffer_.Draw();
}

void plb_LightmapsBuilder::LoadLightPassShaders()
{
	point_light_pass_shader_.ShaderSource(
		rLoadShader( "point_light_pass_f.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_v.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_g.glsl", g_glsl_version));
	SetupLevelVertexAttributes(point_light_pass_shader_);
	point_light_pass_shader_.Create();

	point_light_shadowmap_shader_.ShaderSource(
		rLoadShader( "point_light_shadowmap_f.glsl", g_glsl_version),
		rLoadShader( "point_light_shadowmap_v.glsl", g_glsl_version),
		rLoadShader( "point_light_shadowmap_g.glsl", g_glsl_version));
	SetupLevelVertexAttributes(point_light_shadowmap_shader_);
	point_light_shadowmap_shader_.Create();

	secondary_light_pass_shader_.ShaderSource(
		rLoadShader( "secondary_light_pass_f.glsl", g_glsl_version),
		rLoadShader( "secondary_light_pass_v.glsl", g_glsl_version),
		rLoadShader( "secondary_light_pass_g.glsl", g_glsl_version));
	SetupLevelVertexAttributes(secondary_light_pass_shader_);
	secondary_light_pass_shader_.Create();

	shadowmap_shader_.ShaderSource(
		"", // No fragment shader
		rLoadShader( "shadowmap_v.glsl", g_glsl_version));
	SetupLevelVertexAttributes(shadowmap_shader_);
	shadowmap_shader_.Create();

	directional_light_pass_shader_.ShaderSource(
		rLoadShader( "sun_light_pass_f.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_v.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_g.glsl", g_glsl_version));
	SetupLevelVertexAttributes(directional_light_pass_shader_);
	directional_light_pass_shader_.Create();

	cone_light_pass_shader_.ShaderSource(
		rLoadShader( "cone_light_pass_f.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_v.glsl", g_glsl_version),
		rLoadShader( "point_light_pass_g.glsl", g_glsl_version));
	SetupLevelVertexAttributes(cone_light_pass_shader_);
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
	texture_show_shader_.SetAttribLocation( "pos", Attrib::Pos );
	texture_show_shader_.SetAttribLocation( "tex_coord", Attrib::TexCoord );
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
	cubemap_show_buffer_.VertexAttribPointer( Attrib::Pos, 2, GL_FLOAT, false, 0);
	cubemap_show_buffer_.VertexAttribPointer( Attrib::TexCoord, 2, GL_FLOAT, false, sizeof(float)*2 );
	cubemap_show_buffer_.SetPrimitiveType(GL_TRIANGLES);
}

void plb_LightmapsBuilder::GenPointlightShadowmap( const m_Vec3& light_pos )
{
	glViewport( 0, 0, point_light_shadowmap_cubemap_.size, point_light_shadowmap_cubemap_.size );

	glBindFramebuffer( GL_FRAMEBUFFER, point_light_shadowmap_cubemap_.fbo_id );
	glClear( GL_DEPTH_BUFFER_BIT );

	m_Mat4 final_matrices[6];
	GenCubemapMatrices( light_pos, final_matrices );
	
	point_light_shadowmap_shader_.Bind();
	point_light_shadowmap_shader_.Uniform( "view_matrices", final_matrices, 6 );

	point_light_shadowmap_shader_.Uniform( "inv_max_light_dst", 1.0f / point_light_shadowmap_cubemap_.max_light_distance );

	polygons_vbo_.Draw();

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
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

	polygons_vbo_.Draw();

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

		for( unsigned int i= 0; i< 6; i++ )
		{
			GenCubemapSideDirectionMultipler(
				direction_multiplier_cubemap_size,
				multipler_data.data(), i );

			glTexImage2D(
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_R8,
				direction_multiplier_cubemap_size, direction_multiplier_cubemap_size,
				0, GL_RED, GL_UNSIGNED_BYTE, multipler_data.data() );
		}
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

void plb_LightmapsBuilder::SecondaryLightPass( const m_Vec3& pos, const m_Vec3& normal )
{
	glViewport( 0, 0, secondary_light_pass_cubemap_.size, secondary_light_pass_cubemap_.size );
	glBindFramebuffer( GL_FRAMEBUFFER, secondary_light_pass_cubemap_.fbo_id );
	glClearColor ( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );

	glEnable(GL_CLIP_DISTANCE0);

	// matrices generation
	m_Mat4 final_matrices[6];
	GenCubemapMatrices( pos, final_matrices, 3.1415926535f * 0.5f );

	// bind lightmap texture
	glActiveTexture( GL_TEXTURE0 + 0 );
	glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.tex_id );

	// bind cubemap texture
	unsigned int arrays_bindings_unit= 3;
	textures_manager_->BindTextureArrays(arrays_bindings_unit);
	int textures_uniform[32];
	for( unsigned int i= 0; i< textures_manager_->ArraysCount(); i++ )
		textures_uniform[i]= arrays_bindings_unit + i;

	secondary_light_pass_shader_.Bind();
	secondary_light_pass_shader_.Uniform( "textures", textures_uniform, textures_manager_->ArraysCount() );
	secondary_light_pass_shader_.Uniform( "lightmap", int(0) );
	secondary_light_pass_shader_.Uniform( "view_matrices", final_matrices, 6 );
	secondary_light_pass_shader_.Uniform( "clip_plane", normal.x, normal.y, normal.z, -(normal * pos) );

	polygons_vbo_.Draw();

	glDisable(GL_CLIP_DISTANCE0);

	r_Framebuffer::BindScreenFramebuffer();
}

void plb_LightmapsBuilder::GenDirectionalLightShadowmap( const m_Mat4& shadow_mat )
{
	glDisable( GL_CULL_FACE );

	directional_light_shadowmap_.Bind();
	glClear( GL_DEPTH_BUFFER_BIT );

	shadowmap_shader_.Bind();
	shadowmap_shader_.Uniform( "view_matrix", shadow_mat );

	polygons_vbo_.Draw();

	r_Framebuffer::BindScreenFramebuffer();

	glEnable( GL_CULL_FACE );
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

	polygons_vbo_.Draw();

	r_Framebuffer::BindScreenFramebuffer();

	glEnable( GL_CULL_FACE );
	glDisable( GL_BLEND );
}

void plb_LightmapsBuilder::GenConeLightShadowmap( const m_Mat4& shadow_mat )
{
	glDisable( GL_CULL_FACE );

	cone_light_shadowmap_.Bind();
	glClear( GL_DEPTH_BUFFER_BIT );

	shadowmap_shader_.Bind();
	shadowmap_shader_.Uniform( "view_matrix", shadow_mat );

	polygons_vbo_.Draw();

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

	polygons_vbo_.Draw();

	r_Framebuffer::BindScreenFramebuffer();

	glEnable( GL_CULL_FACE );
	glDisable( GL_BLEND );
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
		const plb_ImageInfo img= level_data_.textures[ polygon.texture_id ];
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
			const plb_ImageInfo& img= level_data_.textures[ curve.texture_id ];
			for( unsigned int v= curve.first_vertex_number; v< curve.first_vertex_number + curve.grid_size[0] * curve.grid_size[1]; v++ )
			{
				v_p[v].tex_maps[0]= img.texture_array_id;
				v_p[v].tex_maps[1]= img.texture_layer_id;
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

		m_Vec3 world_to_lightmap_basis_u= m_Vec3(polygon.lightmap_basis[0]);
		world_to_lightmap_basis_u/= world_to_lightmap_basis_u.SquareLength();

		m_Vec3 world_to_lightmap_basis_v= m_Vec3(polygon.lightmap_basis[1]);
		world_to_lightmap_basis_v/= world_to_lightmap_basis_v.SquareLength();

		for( unsigned int v= polygon.first_vertex_number; v< polygon.first_vertex_number + polygon.vertex_count; v++ )
		{
			const m_Vec3 rel_pos= m_Vec3( v_p[v].pos ) - m_Vec3( polygon.lightmap_pos );
			float uv[2]= {
				world_to_lightmap_basis_u * rel_pos,
				world_to_lightmap_basis_v * rel_pos };
			if( uv[0] > max_uv[0] ) max_uv[0]= uv[0];
			if( uv[1] > max_uv[1] ) max_uv[1]= uv[1];
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
			float tmp[4];
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
	unsigned int lightmaps_offset= config_.secondary_lightmap_scaler;
	unsigned int lightmap_size[2]= { config_.lightmaps_atlas_size[0], config_.lightmaps_atlas_size[1] };
	unsigned int current_lightmap_atlas_id= 0;
	unsigned int current_column_x= lightmaps_offset;
	unsigned int current_column_y= lightmaps_offset;
	unsigned int current_column_height= sorted_lightmaps.front()->size[1];

	for( plb_SurfaceLightmapData* const lightmap : sorted_lightmaps )
	{
		const unsigned int atlas_width=
			( lightmap->size[0] + config_.secondary_lightmap_scaler - 1 ) /
			config_.secondary_lightmap_scaler * config_.secondary_lightmap_scaler;

		if( current_column_x + atlas_width + lightmaps_offset >= lightmap_size[0] )
		{
			const unsigned int atlas_height=
				( lightmap->size[1] + config_.secondary_lightmap_scaler - 1 ) /
				config_.secondary_lightmap_scaler * config_.secondary_lightmap_scaler;

			current_column_x= lightmaps_offset;
			current_column_y+= current_column_height + lightmaps_offset;
			current_column_height= atlas_height;

			if( current_column_height + current_column_y + lightmaps_offset >= lightmap_size[1] )
			{
				current_lightmap_atlas_id++;
				current_column_y= lightmaps_offset;
			}
		}
		
		lightmap->coord[0]= current_column_x;
		lightmap->coord[1]= current_column_y;
		lightmap->atlas_id= current_lightmap_atlas_id;

		current_column_x+= atlas_width + lightmaps_offset;
	}// for polygons

	lightmap_atlas_texture_.size[0]= config_.lightmaps_atlas_size[0];
	lightmap_atlas_texture_.size[1]= config_.lightmaps_atlas_size[1];
	lightmap_atlas_texture_.size[2]= current_lightmap_atlas_id+1;
}


void plb_LightmapsBuilder::CreateLightmapBuffers()
{
	unsigned int lightmap_size[2]= { config_.lightmaps_atlas_size[0], config_.lightmaps_atlas_size[1] };

	unsigned char* lightmap_data= new unsigned char[ lightmap_size[0] * lightmap_size[1] * 4 ];

	//secondary ambient lightmap textures
	for( unsigned int i= 0; i< 1; i++ )
	{
		glGenTextures( 1, &lightmap_atlas_texture_.secondary_tex_id[i] );
		glBindTexture( GL_TEXTURE_2D_ARRAY, lightmap_atlas_texture_.secondary_tex_id[i] );
		glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32F,
			lightmap_size[0] / config_.secondary_lightmap_scaler, lightmap_size[1] / config_.secondary_lightmap_scaler,
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
		m_Vec3 world_to_lightmap_basis_u= m_Vec3(poly.lightmap_basis[0]);
		world_to_lightmap_basis_u/= world_to_lightmap_basis_u.SquareLength();

		m_Vec3 world_to_lightmap_basis_v= m_Vec3(poly.lightmap_basis[1]);
		world_to_lightmap_basis_v/= world_to_lightmap_basis_v.SquareLength();

		for( unsigned int v= poly.first_vertex_number; v< poly.first_vertex_number + poly.vertex_count; v++ )
		{
			const m_Vec3 rel_pos= m_Vec3( v_p[v].pos ) - m_Vec3( poly.lightmap_pos );

			v_p[v].lightmap_coord[0]= world_to_lightmap_basis_u * rel_pos;
			v_p[v].lightmap_coord[0]+= float(poly.lightmap_data.coord[0]);
			v_p[v].lightmap_coord[0]*= inv_lightmap_size[0];

			v_p[v].lightmap_coord[1]= world_to_lightmap_basis_v * rel_pos;
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
