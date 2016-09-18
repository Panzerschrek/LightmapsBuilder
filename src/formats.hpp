#pragma once
#include <vector>
#include <string>

// 32bit GPU vertex
struct plb_Vertex
{
	float pos[3];
	float tex_coord[2];

	// For luminous polygons lightmap_coord[0] is luminosity
	float lightmap_coord[2];

	// 0 - texture array number
	// 1 - layer number inside texture array
	// 2 - lightmap layer number
	// 3 - unused
	unsigned char tex_maps[4];
};

typedef std::vector<plb_Vertex> plb_Vertices;

//4b GPU low-precision normal
struct plb_Normal
{
	char xyz[3];
	char reserved;
};

typedef std::vector<plb_Normal>plb_Normals;

struct plb_SurfaceLightmapData
{
	unsigned int atlas_id;
	unsigned short coord[2]; // coordinates (u,v) of corner
	unsigned short size[2]; // width and hight of lightmap
};

namespace plb_SurfaceFlags
{
	enum : unsigned int
	{
		NoLightmap= 1u << 0u,
		NoShadow= 1u << 1u,
	};
}

struct plb_Polygon
{
	float texture_basis[2][4];

	// Basis for conversion of lightmap coord to world coord
	float lightmap_basis[2][3];
	float lightmap_pos[3];

	float normal[3];
	unsigned int flags;

	unsigned int first_vertex_number;
	unsigned int vertex_count;

	unsigned int first_index;
	unsigned int index_count;

	unsigned int material_id;// number of material in input materials vector

	plb_SurfaceLightmapData lightmap_data;
};

typedef std::vector<plb_Polygon> plb_Polygons;

struct plb_CurvedSurface
{
	unsigned int grid_size[2]; // must be n*2+1, n > 0
	float bb_min[3];
	float bb_max[3];
	unsigned int first_vertex_number;

	unsigned int flags;

	unsigned int material_id;// number of material in input materials vector

	plb_SurfaceLightmapData lightmap_data;
};

typedef std::vector<plb_CurvedSurface> plb_CurvedSurfaces;

struct plb_BuildInImage
{
	std::string name;
	unsigned int size[2];
	std::vector<unsigned char> data_rgba;
};

typedef std::vector<plb_BuildInImage> plb_BuildInImages;

struct plb_Material
{
	std::string albedo_texture_file_name;
	std::string light_texture_file_name;

	unsigned int albedo_texture_number;
	unsigned int light_texture_number;

	float luminosity= 0.0f;
	bool split_to_point_lights= false;
	bool cast_alpha_shadow= false;
};

typedef std::vector<plb_Material> plb_Materials;

struct plb_ImageInfo
{
	unsigned int texture_array_id;
	unsigned int texture_layer_id;

	unsigned int size_log2[2]; // size after transforming
	unsigned int original_size[2]; // size of original image

	std::string file_name;
};

typedef std::vector<plb_ImageInfo> plb_ImageInfos;

struct plb_PointLight
{
	float pos[3];
	float intensity;
	unsigned char color[3];
	unsigned char reserved;
};

typedef std::vector<plb_PointLight> plb_PointLights;

struct plb_DirectionalLight
{
	float direction[3]; // normalized direction to light source
	float intensity;
	unsigned char color[3];
	unsigned char reserved;
};

typedef std::vector<plb_DirectionalLight> plb_DirectionalLights;

struct plb_ConeLight : public plb_PointLight
{
	float direction[3]; // normalized direction from light source
	float angle;
};

typedef std::vector<plb_ConeLight> plb_ConeLights;

// Light of small luminous surface.
// Light intensity depends on cosine of angle between direction to light and light sample normal.
struct plb_SurfaceSampleLight : public plb_PointLight
{
	float normal[3];
};

typedef std::vector<plb_SurfaceSampleLight> plb_SurfaceSampleLights;

// Konfig postroitelä.
// Parametry po umolcaniju blizki k prijemlemym.
// Parametry sledujet podbiratj ishodä iz zadaci,
// dlä kotoroj ispoljzyjetsä postroitelj svetokart.
struct plb_Config
{
	enum class SourceDataType
	{
		Quake1BSP,
		Quake2BSP,
		Quake3BSP,
		HalfLifeBSP,
	};

	SourceDataType source_data_type= SourceDataType::Quake3BSP;

	// Putj k teksturam na fajlovoj sisteme.
	std::string textures_path;

	// Gamma of laoded textures.
	// Values above 1 - make textures darker, values less, then 1, makes textures brighter.
	float textures_gamma= 1.0f;

	// Razmery tekstur urovnä.
	// Vse zagružennyje tekstury privodätsä k kvadratam stepeni dvojki i
	// preobrazujutsä v bližajšij podhodäscij razmer.
	unsigned int min_textures_size_log2= 6;
	unsigned int max_textures_size_log2= 8;

	// Otnošenije masštaba tekstury k masštaby svetokarty pri rascöte.
	// log2( texture_texels / lightmap_texels ).
	// Ispoljzujetsä toljko jesli vo vhodnyh dannyh netu informaçii o naloženii svetokart.
	unsigned int inv_lightmap_scale_log2= 2;

	// Otnošenije razmera tekselä originaljnoj svetokarty k razmeru tekselä svetokarty,
	// ispoljzujemoj pri rascöte.
	// Ispoljzujetsä toljko jesli vo vhodnyh dannyh jestj informaçija o naloženii svetokart.
	unsigned int lightmap_scale_to_original= 4;

	// Parametry tocnosti rascöta.
	unsigned int point_light_shadowmap_cubemap_size_log2= 10;
	unsigned int directional_light_shadowmap_size_log2= 11;
	unsigned int cone_light_shadowmap_size_log2= 10;
	unsigned int secondary_light_pass_cubemap_size_log2= 7;

	// Maksimaljnaja svetimostj poverhnosti, pri kotoroj svet ot nejo jescö
	// risujetsä rasterizaçijej na stadii vtoricnogo osvescenija.
	// Poverhnsoti s boljšej jarkostju budut razbivatjsä na tocecnyje istocniki sveta.
	float max_luminocity_for_direct_luminous_surfaces_drawing= 50.0f;

	// Na skoljko castej nužno delitj odin metr jarkoj svetäscejsä poverhnosti.
	// Cem boljše eto cislo, tem boljše budet sozdano istocnikov sveta ot poverhnosti.
	float luminous_surfaces_tessellation_inv_size= 8.0f;

	// Razmer boljšoj tekstury, gde razmescajutsä svetokarty otdeljnyh poverhnostej.
	// Želateljno, ctoby razmer po osi X byl ne ocenj boljšim, ctoby stroki karty osvescenija
	// nahodilisj blizko v pamäti, no i ne ocenj malenjkim - ctoby vlezli samyje boljšije poverhnosti.
	unsigned int lightmaps_atlas_size[2]= { 512, 2048 };

	// Otnošenije razmera ishodnoj karty osvescenija k karte osvescenija ot vtoricnyh istocnikov.
	unsigned int secondary_lightmap_scaler= 4;

	// Ispoljzovatj li usrednenije çveta tekstury svetäscejsä poverhnosti pri rascöte
	// sveta ot nejo.
	bool use_average_texture_color_for_luminous_surfaces= true;
};

struct plb_LevelData
{
	// Vertices for common polygons, sky polygons.
	plb_Vertices vertices;

	plb_Polygons polygons;
	std::vector<unsigned int> polygons_indeces;

	plb_Polygons sky_polygons;
	std::vector<unsigned int> sky_polygons_indeces;

	plb_PointLights point_lights;
	plb_DirectionalLights directional_lights;
	plb_ConeLights cone_lights;

	plb_BuildInImages build_in_images;
	plb_Materials materials;
	plb_ImageInfos textures;

	plb_CurvedSurfaces curved_surfaces;
	plb_Vertices curved_surfaces_vertices;
};
