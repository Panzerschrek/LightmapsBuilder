#pragma once

#include <chrono>

#include <vec.hpp>
#include <matrix.hpp>

class plb_CameraController final
{
public:
	plb_CameraController(
		const m_Vec3& pos= m_Vec3(0.0f, 0.0f, 0.0f),
		const m_Vec2& angle= m_Vec2(0.0f, 0.0f),
		float aspect= 1.0f, float fov= 3.1415926535f*0.5f );

	~plb_CameraController();

	void Tick();

	void GetViewMatrix( m_Mat4* out_mat ) const;

	m_Vec3 GetCamPos() const { return pos_; }

	void RotateX( int delta );
	void RotateY( int delta );

	void ForwardPressed();
	void BackwardPressed();
	void LeftPressed();
	void RightPressed();
	void ForwardReleased();
	void BackwardReleased();
	void LeftReleased();
	void RightReleased();

	void UpPressed();
	void DownPressed();
	void UpReleased();
	void DownReleased();

	void RotateUpPressed();
	void RotateDownPressed();
	void RotateLeftPressed();
	void RotateRightPressed();
	void RotateUpReleased();
	void RotateDownReleased();
	void RotateLeftReleased();
	void RotateRightReleased();

private:
	m_Vec3 pos_;
	m_Vec2 angle_;

	float aspect_;
	float fov_;

	bool forward_pressed_, backward_pressed_, left_pressed_, right_pressed_;
	bool up_pressed_, down_pressed_;
	bool rotate_up_pressed_, rotate_down_pressed_, rotate_left_pressed_, rotate_right_pressed_;

	std::chrono::steady_clock::time_point prev_calc_tick_;
};
