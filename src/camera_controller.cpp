#include "camera_controller.hpp"

plb_CameraController::plb_CameraController(
	const m_Vec3& pos,
	const m_Vec2& angle,
	float aspect, float fov )
	: pos_(pos), angle_(angle), aspect_(aspect), fov_(fov)
	, forward_pressed_(false), backward_pressed_(false), left_pressed_(false), right_pressed_(false)
	, up_pressed_(false), down_pressed_(false)
	, rotate_up_pressed_(false), rotate_down_pressed_(false), rotate_left_pressed_(false), rotate_right_pressed_(false)
	, prev_calc_tick_(std::chrono::steady_clock::now())
{}

plb_CameraController::~plb_CameraController()
{}

void plb_CameraController::Tick()
{
	const auto new_tick= std::chrono::steady_clock::now();

	const float dt_s=
		static_cast<float>( std::chrono::duration_cast<std::chrono::milliseconds>(new_tick - prev_calc_tick_).count() ) *
		0.001f;

	prev_calc_tick_= new_tick;

	m_Vec2 rotate_vec(0.0f,0.0f);
	if(rotate_up_pressed_) rotate_vec.x+= -1.0f;
	if(rotate_down_pressed_) rotate_vec.x+= 1.0f;
	if(rotate_left_pressed_) rotate_vec.y+= -1.0f;
	if(rotate_right_pressed_) rotate_vec.y+= 1.0f;

	const float rot_speed= 1.75f;
	angle_+= dt_s * rot_speed * rotate_vec;
	
	if( angle_.y > plb_Constants::two_pi ) angle_.y-= plb_Constants::two_pi;
	else if( angle_.y < 0.0f ) angle_.y+= plb_Constants::two_pi;
	if( angle_.x > plb_Constants::half_pi ) angle_.x= plb_Constants::half_pi;
	else if( angle_.x < -plb_Constants::half_pi ) angle_.x= -plb_Constants::half_pi;

	m_Vec3 move_vector(0.0f,0.0f,0.0f);

	if(forward_pressed_) move_vector.z+= 1.0f;
	if(backward_pressed_) move_vector.z+= -1.0f;
	if(left_pressed_) move_vector.x+= -1.0f;
	if(right_pressed_) move_vector.x+= 1.0f;
	if(up_pressed_) move_vector.y+= 1.0f;
	if(down_pressed_) move_vector.y+= -1.0f;

	m_Mat4 move_vector_rot_mat;
	move_vector_rot_mat.RotateY(angle_.y);

	move_vector= move_vector * move_vector_rot_mat;

	const float speed= 5.0f;
	pos_+= dt_s * speed * move_vector;
}

void plb_CameraController::GetViewMatrix( m_Mat4& out_mat ) const
{
	m_Mat4 rot_x, rot_y, translate, perspective;

	translate.Translate( -pos_ );
	rot_x.RotateX( -angle_.x );
	rot_y.RotateY( -angle_.y );
	perspective.PerspectiveProjection( aspect_, fov_, 0.25f, 256.0f );

	out_mat= translate * rot_y * rot_x * perspective;
}

m_Vec3 plb_CameraController::GetCamDir() const
{
	m_Mat4 rot_x, rot_y;
	rot_x.RotateX( angle_.x );
	rot_y.RotateY( angle_.y );

	return m_Vec3( 0.0f, 0.0f, 1.0f ) * rot_x * rot_y;
}

void plb_CameraController::RotateX( int delta )
{
	angle_.x+= float(delta) * 0.01f;
}

void plb_CameraController::RotateY( int delta )
{
	angle_.y+= float(delta) * 0.01f;
}

void plb_CameraController::ForwardPressed()
{
	forward_pressed_= true;
}

void plb_CameraController::BackwardPressed()
{
	backward_pressed_= true;
}

void plb_CameraController::LeftPressed()
{
	left_pressed_= true;
}

void plb_CameraController::RightPressed()
{
	right_pressed_= true;
}

void plb_CameraController::ForwardReleased()
{
	forward_pressed_= false;
}

void plb_CameraController::BackwardReleased()
{
	backward_pressed_= false;
}

void plb_CameraController::LeftReleased()
{
	left_pressed_= false;
}

void plb_CameraController::RightReleased()
{
	right_pressed_= false;
}

void plb_CameraController::UpPressed()
{
	up_pressed_= true;
}

void plb_CameraController::DownPressed()
{
	down_pressed_= true;
}

void plb_CameraController::UpReleased()
{
	up_pressed_= false;
}

void plb_CameraController::DownReleased()
{
	down_pressed_= false;
}

void plb_CameraController::RotateUpPressed()
{
	rotate_up_pressed_= true;
}

void plb_CameraController::RotateDownPressed()
{
	rotate_down_pressed_= true;
}

void plb_CameraController::RotateLeftPressed()
{
	rotate_left_pressed_= true;
}

void plb_CameraController::RotateRightPressed()
{
	rotate_right_pressed_= true;
}

void plb_CameraController::RotateUpReleased()
{
	rotate_up_pressed_= false;
}

void plb_CameraController::RotateDownReleased()
{
	rotate_down_pressed_= false;
}

void plb_CameraController::RotateLeftReleased()
{
	rotate_left_pressed_= false;
}

void plb_CameraController::RotateRightReleased()
{
	rotate_right_pressed_= false;
}
