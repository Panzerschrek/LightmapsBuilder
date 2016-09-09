#pragma once

#include "formats.hpp"

#include <panzer_ogl_lib.hpp>

class plb_TexturesManager final
{
public:
	plb_TexturesManager(
		const plb_Config& config,
		plb_ImageInfos& images,
		const plb_BuildInImages& build_in_images );

	~plb_TexturesManager();

	void BindTextureArrays( unsigned int base_unit ) const;
	unsigned int ArraysCount() const;

	void GetTextureAverageColor(
		unsigned int textures_array_id,
		unsigned int textures_array_layer,
		unsigned char* out_color_rgba ) const;

private:
	struct TexturesArrayLayer
	{
		unsigned char average_color[4];
	};

	struct TextureArray
	{
		unsigned int size[3];
		GLuint tex_id;

		std::vector<TexturesArrayLayer> textures_data;
	};

	std::vector<TextureArray> textures_arrays_;
};

inline unsigned int plb_TexturesManager::ArraysCount() const
{
	return textures_arrays_.size();
}
