#pragma once
#include"Movement/include/Movements.h"
#include"Input/include/Input.h"
namespace X_Y {
	using MouseCode = Input_t::MouseCode;
	class MouseMoved : public Movement
	{
	public:
		MouseMoved(MovementSender s,const float x, const float y)
			: m_MouseX(x), m_MouseY(y),Movement(s) {
		}
		float GetX() const { return m_MouseX; }
		float GetY() const { return m_MouseY; }
		std::string toString() const override
		{
			std::stringstream ss;
			ss << "MouseMoved: " << m_MouseX << ", " << m_MouseY;
			return ss.str();
		}
		MOVEMENT_CLASS_TYPE(MouseMoved)
		MOVEMENT_CLASS_CATEGORY(MTMouse | MTInput)
	private:
		float m_MouseX, m_MouseY;
	};
	class MouseScrolled : public Movement
	{
	public:
		MouseScrolled(MovementSender s,const float xOffset, const float yOffset)
			: m_XOffset(xOffset), m_YOffset(yOffset),Movement(s) {
		}
		float GetXOffset() const { return m_XOffset; }
		float GetYOffset() const { return m_YOffset; }
		std::string toString() const override
		{
			std::stringstream ss;
			ss << "MouseScrolled: " << GetXOffset() << ", " << GetYOffset();
			return ss.str();
		}
		MOVEMENT_CLASS_TYPE(MouseScrolled)
		MOVEMENT_CLASS_CATEGORY(MTMouse | MTInput)
	private:
		float m_XOffset, m_YOffset;
	};
	class MouseButton : public Movement
	{
	public:
		MouseCode GetMouseButton() const { return m_Button; }
		MOVEMENT_CLASS_CATEGORY(MTMouse | MTInput | MTMouseButton)
	protected:
		MouseButton(MovementSender s,const MouseCode button)
			: m_Button(button),Movement(s) {
		}
		MouseCode m_Button;
	};
	class MouseButtonPressed : public MouseButton
	{
	public:
		MouseButtonPressed(MovementSender s,const MouseCode button)
			: MouseButton(s,button) {
		}
		std::string toString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonPressed: " << m_Button;
			return ss.str();
		}
		MOVEMENT_CLASS_TYPE(MouseButtonPressed)
	};
	class MouseButtonReleased : public MouseButton
	{
	public:
		MouseButtonReleased(MovementSender s,const MouseCode button)
			: MouseButton(s,button) {
		}
		std::string toString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonReleased: " << m_Button;
			return ss.str();
		}
		MOVEMENT_CLASS_TYPE(MouseButtonReleased)
	};
}