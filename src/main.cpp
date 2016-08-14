#include <iostream>

#include <SDL.h>

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

	bool quited= false;
	do
	{
		SDL_Event event;
		while( SDL_PollEvent(&event) )
		{
			switch(event.type)
			{
			case SDL_QUIT:
				quited= true;
				break;

			default:
				break;
			};
		}

		SDL_GL_SwapWindow(window);

	}while(!quited);

	SDL_Quit();
	return 0;
}
