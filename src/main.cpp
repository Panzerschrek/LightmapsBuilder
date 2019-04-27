#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

#include <SDL.h>

#include <glsl_program.hpp>
#include <framebuffer.hpp>
#include <panzer_ogl_lib.hpp>
#include <shaders_loading.hpp>

#include "camera_controller.hpp"
#include "lightmaps_builder.hpp"
#include "loaders_common.hpp"

static void FatalError(const char* message)
{
	std::cout << message << std::endl;
	exit(-1);
}

extern "C" int main(int argc, char *argv[])
{
	// TODO - parse more parameters

	#define EXPECT_ARG if( i == argc - 1 ) FatalError( "Expected argument value" );

	const char* game= "q3";
	const char* map_path= "maps/q3/q3dm1.bsp";
	plb_Config cfg;
	cfg.textures_path= "textures/q3/";
	for( int i= 1; i < argc; ++i )
	{
		if( argv[i][0] == '-' )
		{
			const char* const val= i == argc - 1 ? "" : argv[i+1];
			if( std::strcmp( argv[i], "-game" ) == 0 )
			{
				EXPECT_ARG
				game= val;
					 if( std::strcmp( game, "q1" ) == 0 ) cfg.source_data_type= plb_Config::SourceDataType::Quake1BSP;
				else if( std::strcmp( game, "q2" ) == 0 ) cfg.source_data_type= plb_Config::SourceDataType::Quake2BSP;
				else if( std::strcmp( game, "q3" ) == 0 ) cfg.source_data_type= plb_Config::SourceDataType::Quake3BSP;
				else if( std::strcmp( game, "hl" ) == 0 ) cfg.source_data_type= plb_Config::SourceDataType::HalfLifeBSP;
				else
					FatalError( "unknown game" );
			}
			else if( std::strcmp( argv[i], "-map" ) == 0 )
			{
				EXPECT_ARG
				map_path= val;
			}
			else if( std::strcmp( argv[i], "-textures_dir" ) == 0 )
			{
				EXPECT_ARG
				cfg.textures_path= val;
			}
			else if( std::strcmp( argv[i], "-textures_gamma" ) == 0 )
			{
				EXPECT_ARG
				cfg.textures_gamma= std::max( 0.5f, std::min( float(std::atof( val )), 2.0f ) );
			}
			else if( std::strcmp( argv[i], "-min_textures_size_log2" ) == 0 )
			{
				EXPECT_ARG
				cfg.min_textures_size_log2= std::max( 4, std::min( std::atoi( val ), 8 ) );
			}
			else if( std::strcmp( argv[i], "-max_textures_size_log2" ) == 0 )
			{
				EXPECT_ARG
				cfg.max_textures_size_log2= std::max( 6, std::min( std::atoi( val ), 10 ) );
			}
			else if( std::strcmp( argv[i], "-lightmap_scale_to_original" ) == 0 )
			{
				EXPECT_ARG
				cfg.lightmap_scale_to_original= std::max( 1, std::min( std::atoi( val ), 8 ) );
			}
			else if( std::strcmp( argv[i], "-point_light_shadowmap_cubemap_size_log2" ) == 0 )
			{
				EXPECT_ARG
				cfg.point_light_shadowmap_cubemap_size_log2= std::max( 9, std::min( std::atoi( val ), 11 ) );
			}
			else if( std::strcmp( argv[i], "-directional_light_shadowmap_size_log2" ) == 0 )
			{
				EXPECT_ARG
				cfg.directional_light_shadowmap_size_log2= std::max( 10, std::min( std::atoi( val ), 12 ) );
			}
			else if( std::strcmp( argv[i], "-cone_light_shadowmap_size_log2" ) == 0 )
			{
				EXPECT_ARG
				cfg.cone_light_shadowmap_size_log2= std::max( 9, std::min( std::atoi( val ), 11 ) );
			}
			else if( std::strcmp( argv[i], "-secondary_light_pass_cubemap_size_log2" ) == 0 )
			{
				EXPECT_ARG
				cfg.secondary_light_pass_cubemap_size_log2= std::max( 6, std::min( std::atoi( val ), 9 ) );
			}
			else if( std::strcmp( argv[i], "-max_luminocity_for_direct_luminous_surfaces_drawing" ) == 0 )
			{
				EXPECT_ARG
				cfg.max_luminocity_for_direct_luminous_surfaces_drawing= std::max( 1.0f, std::min( float(std::atof( val )), 100.0f ) );
			}
			else if( std::strcmp( argv[i], "-luminous_surfaces_tessellation_inv_size" ) == 0 )
			{
				EXPECT_ARG
				cfg.luminous_surfaces_tessellation_inv_size= std::max( 2, std::min( std::atoi( val ), 20 ) );
			}
			else if( std::strcmp( argv[i], "-secondary_lightmap_scaler" ) == 0 )
			{
				EXPECT_ARG
				cfg.secondary_lightmap_scaler= std::max( 1, std::min( std::atoi( val ), 8 ) );
			}
			else if( std::strcmp( argv[i], "-use_average_texture_color_for_luminous_surfaces" ) == 0 )
			{
				EXPECT_ARG
				cfg.use_average_texture_color_for_luminous_surfaces= std::atoi( val ) != 0;
			}
			else
				FatalError( ( std::string( "unknown parameter: " ) +  argv[i] ).c_str() );
		}
	}

	if( cfg.max_textures_size_log2 < cfg.min_textures_size_log2 )
		cfg.max_textures_size_log2= cfg.min_textures_size_log2;

	LoadLoaderLibrary( ( std::string(game) + "_loader" ).c_str() );

	if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
		FatalError("Can not initialize sdl video");

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	const int screen_width= 1024, screen_height= 768;

	SDL_Window* const window=
		SDL_CreateWindow(
			"Panzerschrek's Lightmaps Builder",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			screen_width, screen_height,
			SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN );

	if( window == nullptr )
		FatalError( "Can not create window" );

	const SDL_GLContext gl_context= SDL_GL_CreateContext(window);
	if( gl_context == nullptr )
		FatalError( "Can not create OpenGL context" );

	SDL_GL_SetSwapInterval(1);

	GetGLFunctions( SDL_GL_GetProcAddress );

	{ // Shaders errors logging
		const auto shaders_log_callback=
			[]( const char* log_data )
			{
				std::cout << log_data << std::endl;
			};

		rSetShaderLoadingLogCallback( shaders_log_callback );
		r_GLSLProgram::SetProgramBuildLogOutCallback( shaders_log_callback );
	}

	rSetShadersDir( "shaders" );

	r_Framebuffer::SetScreenFramebufferSize( screen_width, screen_height );

	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);

	std::unique_ptr<plb_LightmapsBuilder> lightmaps_builder(
		new plb_LightmapsBuilder( map_path, cfg ) );

	plb_CameraController cam_controller( m_Vec3(0.0f,0.0f,0.0f), m_Vec2(0.0f,0.0f), float(screen_width)/float(screen_height) );
	m_Vec3 prev_pos= cam_controller.GetCamPos();
	m_Vec3 prev_dir= cam_controller.GetCamDir();
	bool force_redraw= true;
	bool preview_allowed= false;

	bool show_primary_lightmap= true;
	bool show_secondary_lightmap= true;
	bool use_textures_in_preview= true;
	bool draw_luminous_surfaces_in_preview= true;
	bool draw_shadowless_surfaces_in_preview= true;
	bool draw_smooth_lightmaps_in_preview= true;

	int brightness_log= 0;

	bool quited= false;

	const auto main_loop_iteration=
	[&]()
	{
		SDL_Event event;
		while( SDL_PollEvent(&event) )
		{
			const int key= event.key.keysym.sym;

			switch(event.type)
			{
			case SDL_QUIT:
				quited= true;
				std::exit(0);
				break;

			case SDL_KEYDOWN:
				switch(key)
				{
				case SDLK_LEFT: cam_controller.RotateLeftPressed(); break;
				case SDLK_RIGHT: cam_controller.RotateRightPressed(); break;
				case SDLK_UP: cam_controller.RotateUpPressed(); break;
				case SDLK_DOWN: cam_controller.RotateDownPressed(); break;
				case SDLK_w: cam_controller.ForwardPressed(); break;
				case SDLK_s: cam_controller.BackwardPressed(); break;
				case SDLK_a: cam_controller.LeftPressed(); break;
				case SDLK_d: cam_controller.RightPressed(); break;
				case SDLK_SPACE: cam_controller.UpPressed(); break;
				case SDLK_c: cam_controller.DownPressed(); break;
				default:break;
				};
				break;

			case SDL_KEYUP:
				switch(key)
				{
				case SDLK_LEFT: cam_controller.RotateLeftReleased(); break;
				case SDLK_RIGHT: cam_controller.RotateRightReleased(); break;
				case SDLK_UP: cam_controller.RotateUpReleased(); break;
				case SDLK_DOWN: cam_controller.RotateDownReleased(); break;
				case SDLK_w: cam_controller.ForwardReleased(); break;
				case SDLK_s: cam_controller.BackwardReleased(); break;
				case SDLK_a: cam_controller.LeftReleased(); break;
				case SDLK_d: cam_controller.RightReleased(); break;
				case SDLK_SPACE: cam_controller.UpReleased(); break;
				case SDLK_c: cam_controller.DownReleased(); break;

				case SDLK_1:
					show_primary_lightmap= !show_primary_lightmap;
					force_redraw= true;
					break;
				case SDLK_2:
					show_secondary_lightmap= !show_secondary_lightmap;
					force_redraw= true;
					break;
				case SDLK_3:
					use_textures_in_preview= !use_textures_in_preview;
					force_redraw= true;
					break;
				case SDLK_4:
					draw_luminous_surfaces_in_preview= !draw_luminous_surfaces_in_preview;
					force_redraw= true;
					break;
				case SDLK_5:
					draw_shadowless_surfaces_in_preview= !draw_shadowless_surfaces_in_preview;
					force_redraw= true;
					break;
				case SDLK_6:
					draw_smooth_lightmaps_in_preview= !draw_smooth_lightmaps_in_preview;
					force_redraw= true;
					break;

				case SDLK_0:
					brightness_log= 0;
					force_redraw= true;
					break;
				case SDLK_MINUS:
					brightness_log--;
					force_redraw= true;
					break;
				case SDLK_PLUS:
				case SDLK_EQUALS:
					brightness_log++;
					force_redraw= true;
					break;
				}
				break;

			default:
				break;
			};
		}

		m_Mat4 view_matrix;
		cam_controller.Tick();
		cam_controller.GetViewMatrix( view_matrix );

		const m_Vec3 new_pos= cam_controller.GetCamPos();
		const m_Vec3 new_dir= cam_controller.GetCamDir();
		if( preview_allowed &&
			( force_redraw || new_pos != prev_pos || new_dir != prev_dir ) )
		{
			force_redraw= false;
			prev_pos= new_pos;
			prev_dir= new_dir;

			glClearColor( 0.3f, 0.0f, 0.3f, 0.0f );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

			lightmaps_builder->DrawPreview(
				view_matrix,
				cam_controller.GetCamPos(),
				cam_controller.GetCamDir(),
				std::pow( 2.0f, float(brightness_log) / 2.0f ),
				show_primary_lightmap,
				show_secondary_lightmap,
				use_textures_in_preview,
				draw_luminous_surfaces_in_preview,
				draw_shadowless_surfaces_in_preview,
				draw_smooth_lightmaps_in_preview );

			SDL_GL_SwapWindow(window);
		}
	};

	main_loop_iteration();

	lightmaps_builder->MakePrimaryLight( main_loop_iteration );
	preview_allowed= true;
	lightmaps_builder->MakeSecondaryLight( main_loop_iteration );

	do
	{
		force_redraw = true;
		main_loop_iteration();
	}while( !quited );

	lightmaps_builder.reset();

	SDL_Quit();
	return 0;
}
