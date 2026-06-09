#pragma once
#include"Math/include/Math.h"
namespace X_Y {
	//制定render模块的数学库
	//其实就是转发
	//日后替换数学库，数学库需要暴露和这里差不多的接口，或者按照glm的协议写新的协议就行
	struct RenderMath :public XMath<GlmMathPolicy> {
		MATH_SELF_NAME_USE(Vec2,Vec2)
		MATH_SELF_NAME_USE(Vec3,Vec3)
		MATH_SELF_NAME_USE(Vec4,Vec4)
		MATH_SELF_NAME_USE(IVec2,IVec2)
		MATH_SELF_NAME_USE(IVec3,IVec3)
		MATH_SELF_NAME_USE(IVec4,IVec4)
		MATH_SELF_NAME_USE(Mat2, Mat2)
		MATH_SELF_NAME_USE(Mat3, Mat3)
		MATH_SELF_NAME_USE(Mat4, Mat4)
		MATH_SELF_NAME_USE(Quat, Quat)

		MATH_SELF_API(Dot,Dot)
	};
}