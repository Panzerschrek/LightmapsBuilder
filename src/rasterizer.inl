#include <algorithm>
#include <cmath>

template<class RasterElement>
plb_Rasterizer<RasterElement>::plb_Rasterizer( const Buffer& buffer )
	: buffer_(buffer)
{
}

template<class RasterElement>
void plb_Rasterizer<RasterElement>::DrawTriangle(
	const m_Vec2* vertices,
	const RasterElement* vertex_attributes )
{
	unsigned int upper_index;
	unsigned int middle_index;
	unsigned int lower_index;


	if( vertices[0].y > vertices[1].y && vertices[0].y > vertices[2].y )
	{
		upper_index= 0;
		lower_index= vertices[1].y < vertices[2].y ? 1u : 2u;
	}
	else if( vertices[1].y > vertices[0].y && vertices[1].y > vertices[2].y )
	{
		upper_index= 1;
		lower_index= vertices[0].y < vertices[2].y ? 0u : 2u;
	}
	else//if( vertices[2].y > vertices[0].y && vertices[2].y > vertices[1].y )
	{
		upper_index= 2;
		lower_index= vertices[0].y < vertices[1].y ? 0u : 1u;
	}
	middle_index= 0u + 1u + 2u - upper_index - lower_index;

	const float long_edge_y_length=  vertices[ upper_index ].y - vertices[ lower_index ].y;

	if( long_edge_y_length <= 0.001f )
		return; // triangle is too small

	const float middle_k=
		( vertices[ middle_index ].y - vertices[ lower_index ].y ) /
		long_edge_y_length;

	const float middle_x=
		vertices[ upper_index ].x * middle_k +
		vertices[ lower_index ].y * (1.0f - middle_k);

	const m_Vec2 middle_vertex( middle_x, vertices[ middle_index ].y );

	const m_Vec2 middle_attribute=
		vertex_attributes[ upper_index ].x * middle_k +
		vertex_attributes[ lower_index ].y * (1.0f - middle_k);

	if( middle_x >= vertices[ middle_index ].x )
	{
		/*
		   /\
		  /  \
		 /    \
		+_     \  <-
		   _    \
			 _   \
			   _  \
				 _ \
				   _\
		*/

		triangle_part_vertices_[0]= &vertices[ lower_index ];
		triangle_part_vertices_[1]= &vertices[ middle_index ];
		triangle_part_vertices_[2]= &vertices[ lower_index ];
		triangle_part_vertices_[3]= &middle_vertex;
		triangle_part_attributes_[0]= &vertex_attributes[ lower_index ];
		triangle_part_attributes_[1]= &vertex_attributes[ middle_index ];
		triangle_part_attributes_[2]= &vertex_attributes[ lower_index ];
		triangle_part_attributes_[3]= &middle_attribute;
		DrawTrianglePart();

		triangle_part_vertices_[0]= &vertices[ middle_index ];
		triangle_part_vertices_[1]= &vertices[ upper_index ];
		triangle_part_vertices_[2]= &middle_vertex;
		triangle_part_vertices_[3]= &vertices[ upper_index ];
		triangle_part_attributes_[0]= &vertex_attributes[ middle_index ];
		triangle_part_attributes_[1]= &vertex_attributes[ upper_index ];
		triangle_part_attributes_[2]= &middle_attribute;
		triangle_part_attributes_[3]= &vertex_attributes[ upper_index ];
		DrawTrianglePart();
	}
	else
	{
		/*
				/\
			   /  \
			  /    \
		->   /     _+
			/    _
		   /   _
		  /  _
		 / _
		/_
		*/
		triangle_part_vertices_[0]= &vertices[ lower_index ];
		triangle_part_vertices_[1]= &middle_vertex;
		triangle_part_vertices_[2]= &vertices[ lower_index ];
		triangle_part_vertices_[3]= &vertices[ middle_index ];
		triangle_part_attributes_[0]= &vertex_attributes[ lower_index ];
		triangle_part_attributes_[1]= &middle_attribute;
		triangle_part_attributes_[2]= &vertex_attributes[ lower_index ];
		triangle_part_attributes_[3]= &vertex_attributes[ middle_index ];
		DrawTrianglePart();

		triangle_part_vertices_[0]= &middle_vertex;
		triangle_part_vertices_[1]= &vertices[ upper_index ];
		triangle_part_vertices_[2]= &vertices[ middle_index ];
		triangle_part_vertices_[3]= &vertices[ upper_index ];
		triangle_part_vertices_[0]= &middle_attribute;
		triangle_part_attributes_[1]= &vertex_attributes[ upper_index ];
		triangle_part_attributes_[2]= &vertex_attributes[ middle_index ];
		triangle_part_attributes_[3]= &vertex_attributes[ upper_index ];
		DrawTrianglePart();
	}
}

template<class RasterElement>
void plb_Rasterizer<RasterElement>::DrawTrianglePart()
{
	const float dy= triangle_part_vertices_[1]->y - triangle_part_vertices_[0]->y;
	const float dx_left = triangle_part_vertices_[1]->x - triangle_part_vertices_[0]->x;
	const float dx_right= triangle_part_vertices_[3]->x - triangle_part_vertices_[2]->x;
	const float x_left_step = dx_left  / dy;
	const float x_right_step= dx_right / dy;

	const float attrib_y_step= 1.0f / dy;

	const int y_start= std::max( 0,
			static_cast<int>( std::round( triangle_part_vertices_[0]->y ) ) );

	const int y_end= std::min( int(buffer_.size[1]),
		static_cast<int>( std::round( triangle_part_vertices_[1]->y ) ) );

	const float y_cut= float(y_start) + 0.5f - triangle_part_vertices_[0]->y;
	float x_left = triangle_part_vertices_[0]->x + y_cut * x_left_step ;
	float x_right= triangle_part_vertices_[2]->x + y_cut * x_right_step;
	float attrib_y= y_cut * attrib_y_step;
	for(
		int y= y_start;
		y< y_end;
		y++, x_left+= x_left_step, x_right+= x_right_step, attrib_y+= attrib_y_step )
	{
		const RasterElement attrib_left =
			*triangle_part_attributes_[1] * attrib_y +
			*triangle_part_attributes_[0] * (1.0f - attrib_y);
		const RasterElement attrib_right=
			*triangle_part_attributes_[3] * attrib_y +
			*triangle_part_attributes_[2] * (1.0f - attrib_y);

		const float attrib_x_step= 1.0f / ( x_right - x_left );

		const int x_start= std::max( 0,
			static_cast<int>( std::round( x_left ) ) );

		const int x_end= std::min( int(buffer_.size[0]),
			static_cast<int>( std::round( x_right ) ) );

		const float x_cut= float(x_start) + 0.5f - x_right;
		float attrib_x= x_cut * attrib_x_step;

		RasterElement* dst= x_start + buffer_.data + y * buffer_.size[0];
		for( int x= x_start; x < x_end; x++, attrib_x+= attrib_x_step, dst++ )
		{
			const RasterElement attrib =
				attrib_left * attrib_x +
				attrib_right * (1.0f - attrib_x);
			*dst= attrib;
		}
	} // for y
}
