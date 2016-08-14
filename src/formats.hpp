﻿#pragma once
#include <vector>
#include <string>

// 32bit GPU vertex
struct plb_Vertex
{
	float pos[3];
	float tex_coord[2];
	float lightmap_coord[2];

	// 0 - texture array number
	// 1 - layer number inside texture array
	// 2 - lightmap layer number
	// 3 - unused
	unsigned char tex_maps[4];
};

//4b GPU low-precision normal
struct plb_Normal
{
	char xyz[3];
	char reserved;
};

struct plb_SurfaceLightmapData
{
	unsigned int atlas_id;
	unsigned short coord[2]; // coordinates (u,v) of corner
	unsigned short size[2]; // width and hight of lightmap
};

struct plb_Polygon
{
	float texture_basis[2][4];
	float lightmap_basis[2][4];
	float normal[3];
	unsigned int flags;
	unsigned int first_vertex_number;
	unsigned int vertex_count;
	unsigned int texture_id;// number of texture in input textures vector

	plb_SurfaceLightmapData lightmap_data;
};

struct plb_CurvedSurface
{
	unsigned int grid_size[2]; // must be n*2+1, n > 0
	float bb_min[3];
	float bb_max[3];
	unsigned int first_vertex_number;

	unsigned int texture_id;// number of texture in input textures vector

	plb_SurfaceLightmapData lightmap_data;
};

struct plb_ImageInfo
{
	unsigned int texture_array_id;
	unsigned int texture_layer_id;

	unsigned int size_log2[2]; // size after transforming
	unsigned int original_size[2]; // size of original image
	std::string file_name;
};

struct plb_PointLight
{
	float pos[3];
	float intensity;
	unsigned char color[3];
	unsigned char reserved;
};

struct plb_DirectionalLight
{
	float direction[3];
	float intensity;
	unsigned char color[3];
	unsigned char reserved;
};

struct plb_ConeLinght
{
	float pos[3];
	float direction[3];
	float angle;
	float intensity;
	unsigned char color[3];
	unsigned char reserved[1];
};

struct plb_Config
{
	std::string textures_path;
	unsigned int min_textures_size_log2;
	unsigned int max_textures_size_log2;

	unsigned int max_polygon_lightmap_size; // max size of individual lightmap for polygon

	unsigned int inv_lightmap_scale_log2; // log2( texture_texels / lightmap_texels )
	unsigned int out_inv_lightmap_scale_log2; // size for output lightmaps

	unsigned int lightmaps_atlas_size[2]; // size of big texture, where shall plased textures

	// Otnošenije razmera ishodnoj karty osvescenija k karte osvescenija ot vtoricnyh istocnikov
	unsigned int secondary_lightmap_scaler;
};

struct plb_LevelData
{
	std::vector<plb_Vertex> vertices;
	std::vector<plb_Polygon> polygons;

	std::vector<plb_PointLight> point_lights;
	std::vector<plb_DirectionalLight> directional_lights;
	std::vector<plb_ConeLinght> cone_lights;

	std::vector<plb_ImageInfo> textures;

	std::vector<plb_CurvedSurface> curved_surfaces;
	std::vector<plb_Vertex> curved_surfaces_vertices;
};
