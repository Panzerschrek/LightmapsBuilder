#include <cstring>
#include <iostream>

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

static void GetAverageColor(
	const unsigned char* data_rgba, unsigned int pixel_count,
	unsigned char* out_color )
{
	unsigned int color_sum[4]= { 0u, 0u, 0u, 0u };

	for( unsigned int i= 0; i < pixel_count; i++ )
	{
		const unsigned char* color= data_rgba + ( i << 2 );
		for( unsigned int j= 0; j < 4; j++ )
			color_sum[j]+= color[j];
	}

	for( unsigned int j= 0; j < 4; j++ )
		out_color[j]= color_sum[j] / pixel_count;
}

#if 0
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
}
#endif

static std::string ReplaceExtension( const std::string& path, const char* extension )
{
	std::string::const_reverse_iterator dot_pos= path.rend();

	for( auto it= path.rbegin(); it != path.rend(); ++it )
	{
		if( *it == '.' )
		{
			dot_pos= it;
			break;
		}
	}

	if( dot_pos == path.rend() )
		return path + "." + extension;

	std::string result( path.begin(), path.end() - (dot_pos - path.rbegin()) );
	result+= extension;

	return result;
}

plb_TexturesManager::plb_TexturesManager(
	const plb_Config& config,
	plb_ImageInfos& images,
	const plb_BuildInImages& build_in_images )
{
	static const char* const img_extensions[]=
	{
		"*", // Hack for buildin textures loading
		"bmp", "pcx", "tga", "jpg", "jpeg", "wal",
	};

	unsigned int textures_data_size= 0;

	ilInit();

	const unsigned int square_arrays_count=
		config.max_textures_size_log2 - config.min_textures_size_log2 + 1;
	textures_arrays_.resize( square_arrays_count );
	for( unsigned int i= 0; i< square_arrays_count; i++ )
	{
		textures_arrays_[i].size[0]=
		textures_arrays_[i].size[1]= 1 << (config.min_textures_size_log2 + i );
		textures_arrays_[i].size[2]= 0;
	}

	// Dummy texture
	textures_arrays_[0].size[2]= 1;

	std::vector<ILuint> il_textures_handles( images.size() );

	for( plb_ImageInfo& img : images )
	{
		const unsigned int i= &img - images.data();

		ilGenImages( 1, &il_textures_handles[i] );
		ilBindImage( il_textures_handles[i] );
		bool img_loaded= false;

		for( const char* const extension : img_extensions )
		{
			// Try load buildin image
			if( extension[0] == '*' )
				for( const plb_BuildInImage& build_in_img : build_in_images )
				{
					if( build_in_img.name == img.file_name )
					{
						ilTexImage(
							build_in_img.size[0], build_in_img.size[1], 1,
							4, IL_RGBA, IL_UNSIGNED_BYTE,
							const_cast<unsigned char*>(build_in_img.data_rgba.data() ) );

						img_loaded= true;
						break;
					}
				}

			if( !img_loaded )
			{
				const std::string file_name= config.textures_path + ReplaceExtension( img.file_name, extension );
				img_loaded= ilLoadImage( file_name.c_str() );
			}

			if( img_loaded )
			{
				img.original_size[0]= ilGetInteger( IL_IMAGE_WIDTH  );
				img.original_size[1]= ilGetInteger( IL_IMAGE_HEIGHT );

				for( unsigned int d= 0; d< 2; d++ )
				{
					img.size_log2[d]= PowerOfTwoCeil(img.original_size[d]);
					if( img.size_log2[d] > config.max_textures_size_log2 )
						img.size_log2[d]= config.max_textures_size_log2;
					else if (img.size_log2[d] < config.min_textures_size_log2 )
						img.size_log2[d]= config.min_textures_size_log2;
				}
				img.size_log2[0]= img.size_log2[1]= std::max( img.size_log2[0], img.size_log2[1] );

				// kostylj dlä TGA fajlov v Quake III
				if( std::strcmp(extension, "tga") == 0 )
					iluFlipImage();

				ilConvertImage( IL_RGBA, IL_UNSIGNED_BYTE );
				//iluBuildMipmaps();
				const unsigned int pix= (
					img.original_size[0] / (1<<img.size_log2[0]) +
					img.original_size[1] / (1<<img.size_log2[1]) ) / 2;
				if( pix > 1 )
					iluPixelize(pix);
				iluImageParameter( ILU_FILTER, ILU_LINEAR );
				iluScale( 1<<img.size_log2[0], 1<<img.size_log2[1], 1 );

				const unsigned int array_id= img.size_log2[0] - config.min_textures_size_log2;
				img.texture_array_id= array_id;
				img.texture_layer_id= textures_arrays_[ array_id ].size[2];
				textures_arrays_[ array_id ].size[2]++;

				textures_data_size+= 1<<( img.size_log2[0] + img.size_log2[1] + 2);

				break;
			}// if texture loaded
		}// for formats

		if (!img_loaded)
		{
			img.original_size[0]= img.original_size[1]= 0;
			img.texture_array_id= 0;
			img.texture_layer_id= 0;

			ilDeleteImages( 1, &il_textures_handles[i] );
			std::cout << "warning, texture \"" << img.file_name.c_str() << "\" not found" << std::endl;
		}
	}// for images

	std::cout << "textures data size: " << (textures_data_size>>10) << " kb" << std::endl;

	for( TextureArray& textures_array : textures_arrays_ )
	{
		if( textures_array.size[2] == 0 )
			continue;

		textures_array.textures_data.resize( textures_array.size[2] );

		glGenTextures( 1, &textures_array.tex_id );
		glBindTexture( GL_TEXTURE_2D_ARRAY, textures_array.tex_id );
		glTexImage3D(
			GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8,
			textures_array.size[0], textures_array.size[1], textures_array.size[2],
			0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );

		// Dummy
		if( &textures_array == &textures_arrays_.front() )
		{
			std::vector<unsigned char> dummy_data( 4 * textures_array.size[0] * textures_array.size[1] );
			std::memset( dummy_data.data(), 128, sizeof(unsigned char) * dummy_data.size() );

			glTexSubImage3D(
				GL_TEXTURE_2D_ARRAY, 0,
				0, 0, 0,
				textures_array.size[0], textures_array.size[1], 1,
				GL_RGBA,
				GL_UNSIGNED_BYTE, dummy_data.data() );
		}

		const unsigned int texture_array_number= &textures_array - textures_arrays_.data();
		for( const plb_ImageInfo& img : images )
		{
			if(
				img.texture_array_id == texture_array_number &&
				img.original_size[0] > 0 && img.original_size[1] > 0 )
			{
				const unsigned int image_index= &img - images.data();

				ilBindImage( il_textures_handles[ image_index ] );
				const void* const tex_data= ilGetData();
				const unsigned int channels= ilGetInteger( IL_IMAGE_CHANNELS );

				GetAverageColor(
					static_cast<const unsigned char*>(tex_data),
					textures_array.size[0] * textures_array.size[1],
					textures_array.textures_data[ img.texture_layer_id ].average_color );

				GLenum format= 0;
				if( channels == 1 ) format = GL_RED;
				else if( channels == 2 ) format= GL_RG;
				else if( channels == 3 ) format= GL_RGB;
				else if( channels == 4 ) format= GL_RGBA;

				glTexSubImage3D(
					GL_TEXTURE_2D_ARRAY, 0,
					0, 0, img.texture_layer_id,
					textures_array.size[0], textures_array.size[1], 1,
					format,
					GL_UNSIGNED_BYTE, tex_data );

				ilDeleteImages( 1, &il_textures_handles[ image_index ] );

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
	for( const TextureArray& textures_array : textures_arrays_ )
	{
		if( textures_array.size[2] == 0 )
			continue;

		glDeleteTextures( 1, &textures_array.tex_id );
	}
}

void plb_TexturesManager::BindTextureArrays( const unsigned int base_unit ) const
{
	for( unsigned int i= 0; i< textures_arrays_.size(); i++ )
	{
		if( textures_arrays_[i].size[2] == 0 )
			continue;

		glActiveTexture( GL_TEXTURE0 + base_unit + i );
		glBindTexture( GL_TEXTURE_2D_ARRAY, textures_arrays_[i].tex_id );
	}
}

void plb_TexturesManager::GetTextureAverageColor(
	unsigned int textures_array_id,
	unsigned int textures_array_layer,
	unsigned char* out_color_rgba ) const
{
	std::memcpy(
		out_color_rgba,
		textures_arrays_[ textures_array_id ].textures_data[ textures_array_layer ].average_color,
		4 );
}
