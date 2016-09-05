#pragma once

#include "formats.hpp"

extern "C"
{

#ifdef PLB_DLL_BUILD
#define PLB_DLL_FUNC __declspec(dllexport)

PLB_DLL_FUNC void LoadBsp(
	const char* file_name,
	const plb_Config& config,
	plb_LevelData& level_data );

#else

extern void (*LoadBsp)(
	const char* file_name,
	const plb_Config& config,
	plb_LevelData& level_data );

#endif


} // extern "C"
