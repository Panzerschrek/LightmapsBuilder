#include <IL/il.h>
#include <IL/ilu.h>

#include "textures_manager.hpp"

static unsigned int PowerOfTwoCeil( unsigned int x )
{
	unsigned int p = 0, s= 1;
	while( x > s )
	{
		p++;
		s<<=1;
	}
	return p;
}

static void GenStubTexture( unsigned char* dst, unsigned int width, unsigned char height )
{
	memset( dst, 125, width * height * 4 );

	for( unsigned int y= 0; y< height; y+=4 )
		for( unsigned int x= 0; x< width; x++ )
		{
			unsigned int ind= 4 * (x + y * width);
			dst[ind]= dst[ind+1]= dst[ind+2]= dst[ind+3]= 255;
		}

	for( unsigned int y= 0; y< height; y++ )
		for( unsigned int x= 0; x< width; x=4 )
		{
			unsigned int ind= 4 * (x + y * width);
			dst[ind]= dst[ind+1]= dst[ind+2]= dst[ind+3]= 255;
		}
};

plb_TexturesManager::plb_TexturesManager( const plb_Config* config, std::vector<plb_ImageInfo>* images )
{
	static const char* const img_extensions[]=
	{
		".bmp", ".pcx", ".tga", ".jpg", ".jpeg"
	};

	unsigned int textures_data_size= 0;

	ilInit();

	unsigned int square_arrays_count= config->max_textures_size_log2 - config->min_textures_size_log2 + 1;
	textures_arrays_.resize( square_arrays_count );
	for( unsigned int i= 0; i< square_arrays_count; i++ )
	{
		textures_arrays_[i].size[0]= 
		textures_arrays_[i].size[1]= 1<< (config->min_textures_size_log2 + i );
		textures_arrays_[i].size[2]= 0;
	}

	std::vector<ILuint> il_textures_handles( images->size() );

	for( unsigned int i= 0; i< images->size(); i++ )
	{
		plb_ImageInfo* img= &(*images)[i];

		ilGenImages( 1, &il_textures_handles[i] );
		ilBindImage( il_textures_handles[i] );
		bool img_loaded= false;

		for( unsigned int e= 0; e< sizeof(img_extensions) / sizeof(char*); e++ )
		{
			std::string file_name= config->textures_path + img->file_name + img_extensions[e];
			if( ilLoadImage( file_name.c_str() ) )
			{
				img_loaded= true;
				img->original_size[0]= ilGetInteger( IL_IMAGE_WIDTH );
				img->original_size[1]= ilGetInteger( IL_IMAGE_HEIGHT );

				for( unsigned int d= 0; d< 2; d++ )
				{
					img->size_log2[d]= PowerOfTwoCeil(img->original_size[d]);
					if( img->size_log2[d] > config->max_textures_size_log2 ) img->size_log2[d]= config->max_textures_size_log2;
					else if (img->size_log2[d] < config->min_textures_size_log2 ) img->size_log2[d]= config->min_textures_size_log2;
				}
				img->size_log2[0]= img->size_log2[1]= 
					(img->size_log2[0] > img->size_log2[1]) ? img->size_log2[0] : img->size_log2[1];

				// kostylj dlä TGA fajlov v Quake III
				if( strcmp(img_extensions[e], ".tga") == 0 )
					iluFlipImage();

				ilConvertImage( IL_RGBA, IL_UNSIGNED_BYTE );
				//iluBuildMipmaps();
				unsigned int pix= (
					img->original_size[0] / (1<<img->size_log2[0]) + 
					img->original_size[1] / (1<<img->size_log2[1]) ) / 2;
				if( pix > 1 )
					iluPixelize(pix);
				iluImageParameter( ILU_FILTER, ILU_LINEAR );
				iluScale( 1<<img->size_log2[0], 1<<img->size_log2[1], 1 );

				unsigned int array_id= img->size_log2[0] - config->min_textures_size_log2;
				img->texture_array_id= array_id;
				img->texture_layer_id= textures_arrays_[ array_id ].size[2];
				textures_arrays_[ array_id ].size[2]++;

				textures_data_size+= 1<<( img->size_log2[0] + img->size_log2[1] + 2);
				break;
			}// if texture loaded
		}// for formats
		if (!img_loaded)
		{
			img->original_size[0]= img->original_size[1]= 0;
			img->texture_array_id= 0;
			img->texture_layer_id= 0;

			ilDeleteImages( 1, &il_textures_handles[i] );
			printf( "warning, texture \"%s\" not found\n", img->file_name.c_str() );
		}
	}// for images

	printf( "textures data size: %d kb\n", textures_data_size>>10 );

	for( unsigned int i= 0; i< textures_arrays_.size(); i++ )
	{
		if( textures_arrays_[i].size[2] == 0 ) continue;

		glGenTextures( 1, &textures_arrays_[i].tex_id );
		glBindTexture( GL_TEXTURE_2D_ARRAY, textures_arrays_[i].tex_id );
		glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8,
			textures_arrays_[i].size[0], textures_arrays_[i].size[1], textures_arrays_[i].size[2],
			0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );

		for( std::vector<plb_ImageInfo>::iterator img= images->begin(); img< images->end(); img++ )
		{
			if( img->texture_array_id == i && img->original_size[0] > 0 && img->original_size[1] > 0 )
			{
				ilBindImage( il_textures_handles[ img - images->begin()] );
				void* tex_data= ilGetData();
				unsigned int data_size= ilGetInteger( IL_IMAGE_SIZE_OF_DATA );
				unsigned int channels= ilGetInteger( IL_IMAGE_CHANNELS );

				GLenum format;
				if( channels == 1 ) format = GL_RED;
				else if( channels == 2 ) format= GL_RG;
				else if( channels == 3 ) format= GL_RGB;
				else if( channels == 4 ) format= GL_RGBA;

				glTexSubImage3D( GL_TEXTURE_2D_ARRAY, 0,
					0, 0, img->texture_layer_id,
					textures_arrays_[i].size[0], textures_arrays_[i].size[1], 1,
					format,
					GL_UNSIGNED_BYTE, tex_data );

				ilDeleteImages( 1, &il_textures_handles[ img - images->begin()] );

			}// if image in this array
		}// for images
		glGenerateMipmap( GL_TEXTURE_2D_ARRAY );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}// for textures arrays

	ilShutDown();
}

plb_TexturesManager::~plb_TexturesManager()
{
}

void plb_TexturesManager::BindTextureArrays( unsigned int base_unit )
{
	for( unsigned int i= 0; i< textures_arrays_.size(); i++ )
	{
		if( textures_arrays_[i].size[2] == 0 ) continue;
		glActiveTexture( GL_TEXTURE0 + base_unit + i );
		glBindTexture( GL_TEXTURE_2D_ARRAY, textures_arrays_[i].tex_id );
	}
}
