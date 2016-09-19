#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "loaders_common.hpp"

#ifndef PLB_DLL_BUILD

void (*LoadBsp)(
	const char* file_name,
	const plb_Config& config,
	plb_LevelData& level_data )= nullptr;

bool LoadLoaderLibrary( const char* const library_name )
{
	const char* const c_func_name= "LoadBsp";

	std::string library_file_name= library_name;

#ifdef _WIN32
	library_file_name+= ".dll";

	const HMODULE module= LoadLibraryA( library_file_name.c_str() );
	if( module == NULL )
	{
		std::cout << "Failed to load module \"" << library_file_name << "\"" << std::endl;
		return false;
	}

	LoadBsp= reinterpret_cast<decltype(LoadBsp)>( GetProcAddress( module, c_func_name ) );

	if( LoadBsp == nullptr )
	{
		std::cout << "Failed to get \"" << c_func_name << "\" from \"" << library_file_name << "\"" << std::endl;
		return false;
	}

#else
	library_file_name= "lib" + library_file_name + ".so";

	void* const handle= dlopen( library_file_name.c_str(), RTLD_LAZY );
	if( handle == nullptr )
	{
		std::cout << "Failed to load library \"" << library_file_name << "\"\n"
			<< dlerror() << std::endl;
		return false;
	}

	LoadBsp= reinterpret_cast<decltype(LoadBsp)>( dlsym( handle, c_func_name ) );

	if( LoadBsp == nullptr )
	{
		std::cout << "Failed to get \"" << c_func_name << "\" from \"" << library_file_name << "\"\n"
			<< dlerror() << std::endl;
		return false;
	}

#endif

	return true;
}

#endif//PLB_DLL_BUILD

void plbTransformCoordinatesFromQuakeSystem(
	plb_Polygons& polygons,
	plb_Vertices& vertices,
	std::vector<unsigned int>& indeces,
	std::vector<unsigned int>& sky_indeces )
{
	// transform vertices
	for( plb_Vertex& v : vertices )
	{
		std::swap( v.pos[1], v.pos[2] );
		v.pos[0]*= INV_Q_UNITS_IN_METER;
		v.pos[1]*= INV_Q_UNITS_IN_METER;
		v.pos[2]*= INV_Q_UNITS_IN_METER;
	}

	// transform polygons
	for( plb_Polygon& p : polygons )
	{
		std::swap( p.normal[1], p.normal[2] );

		for( unsigned int i= 0 ; i < 2; i++ )
			std::swap( p.lightmap_basis[i][1], p.lightmap_basis[i][2] );

		std::swap( p.lightmap_pos[1], p.lightmap_pos[2] );

		p.lightmap_basis[0][0]*= INV_Q_UNITS_IN_METER;
		p.lightmap_basis[0][1]*= INV_Q_UNITS_IN_METER;
		p.lightmap_basis[0][2]*= INV_Q_UNITS_IN_METER;
		p.lightmap_basis[1][0]*= INV_Q_UNITS_IN_METER;
		p.lightmap_basis[1][1]*= INV_Q_UNITS_IN_METER;
		p.lightmap_basis[1][2]*= INV_Q_UNITS_IN_METER;
		p.lightmap_pos[0]*= INV_Q_UNITS_IN_METER;
		p.lightmap_pos[1]*= INV_Q_UNITS_IN_METER;
		p.lightmap_pos[2]*= INV_Q_UNITS_IN_METER;
	}

	for( unsigned int i= 0; i < indeces.size(); i+= 3 )
		std::swap( indeces[i], indeces[i+1] );

	for( unsigned int i= 0; i < sky_indeces.size(); i+= 3 )
		std::swap( sky_indeces[i], sky_indeces[i+1] );
}
