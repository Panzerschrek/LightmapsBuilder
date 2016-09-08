#pragma once

#include "formats.hpp"

#define Q_UNITS_IN_METER 64.0f
#define INV_Q_UNITS_IN_METER 0.015625f
#define Q_LIGHT_UNITS_INV_SCALER (1.0f/64.0f)

#ifdef PLB_DLL_BUILD
#define PLB_DLL_FUNC extern "C" __declspec(dllexport)

PLB_DLL_FUNC void LoadBsp(
	const char* file_name,
	const plb_Config& config,
	plb_LevelData& level_data );

#else//PLB_DLL_BUILD

extern void (*LoadBsp)(
	const char* file_name,
	const plb_Config& config,
	plb_LevelData& level_data );

// Loads dynamic library with loader.
// library_name - name of librrary without extension (.so or .dll).
// Returns true on success.
bool LoadLoaderLibrary( const char* const library_name );

#endif//PLB_DLL_BUILD

void plbTransformCoordinatesFromQuakeSystem(
	plb_Polygons& polygons,
	plb_Vertices& vertices,
	std::vector<unsigned int>& indeces,
	std::vector<unsigned int>& sky_indeces );
