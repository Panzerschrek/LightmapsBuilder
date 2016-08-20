#include <iostream>
#include <memory>

#include <SDL.h>

#include <glsl_program.hpp>
#include <framebuffer.hpp>
#include <panzer_ogl_lib.hpp>
#include <shaders_loading.hpp>

#include "camera_controller.hpp"
#include "lightmaps_builder.hpp"

static void FatalError(const char* message)
{
	std::cout << message << std::endl;
	exit(-1);
}

extern "C" int main(int argc, char *argv[])
{
	// TODO - work with parameters
	(void)argc;
	(void)argv;

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
			"Panzerschrek Lightmaps Builder",
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
				std::cout << log_data;
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

	plb_Config cfg;
	cfg.textures_path= "textures/q3/";

	std::unique_ptr<plb_LightmapsBuilder> lightmaps_builder(
		new plb_LightmapsBuilder(
			"maps/q3dm6.bsp", cfg ) );

	plb_CameraController cam_controller( m_Vec3(0.0f,0.0f,0.0f), m_Vec2(0.0f,0.0f), float(screen_width)/float(screen_height) );

	bool quited= false;
	do
	{
		SDL_Event event;
		while( SDL_PollEvent(&event) )
		{
			const int key= event.key.keysym.sym;

			switch(event.type)
			{
			case SDL_QUIT:
				quited= true;
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
				}
				break;

			default:
				break;
			};
		}

		glClearColor( 0.3f, 0.0f, 0.3f, 0.0f );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		m_Mat4 view_matrix;
		cam_controller.Tick();
		cam_controller.GetViewMatrix( view_matrix );

		lightmaps_builder->DrawPreview( view_matrix );

		SDL_GL_SwapWindow(window);

	}while(!quited);

	lightmaps_builder.reset();

	SDL_Quit();
	return 0;
}
