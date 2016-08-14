#pragma once

#include "formats.hpp"

#include <panzer_ogl_lib.hpp>

class plb_TexturesManager final
{
public:
	plb_TexturesManager( const plb_Config* config, plb_ImageInfos& images );
	~plb_TexturesManager();

	void BindTextureArrays( unsigned int base_unit );
	unsigned int ArraysCount() const;

private:
	struct TextureArray
	{
		unsigned int size[3];
		GLuint tex_id;
	};

	std::vector<TextureArray> textures_arrays_;
};

inline unsigned int plb_TexturesManager::ArraysCount() const
{
	return textures_arrays_.size();
}
