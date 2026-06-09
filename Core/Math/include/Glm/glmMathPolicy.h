#pragma once
#include<Math/include/mathPolicy.h>
#include <glm/glm.hpp>
// 矩阵变换：translate/rotate/scale/lookAt/perspective/ortho
#include <glm/gtc/matrix_transform.hpp>
// 向量运算：dot/cross/normalize/length/distance/reflect/refract
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtc/vec1.hpp>
// 角度转换：radians / degrees
#include <glm/gtc/constants.hpp>
// 插值、数值：mix/slerp/clamp/inverse/transpose
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define ALL_GLM_FUNCTIONS \
    MATH_FUNC(Radians,     glm::radians) \
    MATH_FUNC(Degrees,     glm::degrees) \
    MATH_FUNC(Dot,         glm::dot) \
    MATH_FUNC(Cross,       glm::cross) \
    MATH_FUNC(Normalize,   glm::normalize) \
    MATH_FUNC(Length,      glm::length) \
    MATH_FUNC(Distance,    glm::distance) \
    MATH_FUNC(Reflect,     glm::reflect) \
    MATH_FUNC(Refract,     glm::refract) \
    MATH_FUNC(Translate,   glm::translate) \
    MATH_FUNC(Rotate,      glm::rotate) \
    MATH_FUNC(Scale,       glm::scale) \
    MATH_FUNC(LookAt,      glm::lookAt) \
    MATH_FUNC(Perspective, glm::perspective) \
    MATH_FUNC(Ortho,       glm::ortho) \
    MATH_FUNC(Inverse,     glm::inverse) \
    MATH_FUNC(Transpose,   glm::transpose) \
    MATH_FUNC(Slerp,       glm::slerp) \
    MATH_FUNC(Mix,         glm::mix) \
    MATH_FUNC(Clamp,       glm::clamp)


namespace X_Y {
        struct GlmMathPolicy : public MathPolicy
        {
            
            using Vec2 = glm::vec2;
            using Vec3 = glm::vec3;
            using Vec4 = glm::vec4;
            using IVec2 = glm::ivec2;
            using IVec3 = glm::ivec3;
            using IVec4 = glm::ivec4;
            using Mat2 = glm::mat2;
            using Mat3 = glm::mat3;
            using Mat4 = glm::mat4;
            using Quat = glm::quat;

            ALL_GLM_FUNCTIONS
        };
}
#define GLM_NAME_LIST \
    MATH_NAME_USE(Vec2) \
    MATH_NAME_USE(Vec3) \
    MATH_NAME_USE(Vec4) \
    MATH_NAME_USE(IVec2) \
    MATH_NAME_USE(IVec3) \
    MATH_NAME_USE(IVec4) \
    MATH_NAME_USE(Mat2) \
    MATH_NAME_USE(Mat3) \
    MATH_NAME_USE(Mat4) \
    MATH_NAME_USE(Quat)

#define GLM_API_LIST \
  MATH_API(Radians) \
  MATH_API(Degrees) \
  MATH_API( Dot) \
  MATH_API(Cross) \
  MATH_API( Normalize) \
  MATH_API( Length) \
  MATH_API( Distance) \
  MATH_API( Reflect) \
  MATH_API( Refract) \
  MATH_API( Translate) \
  MATH_API( Rotate) \
  MATH_API( Scale) \
  MATH_API( LookAt) \
  MATH_API( Perspective) \
  MATH_API( Ortho) \
  MATH_API( Inverse) \
  MATH_API( Transpose) \
  MATH_API( Slerp) \
  MATH_API( Mix) \
  MATH_API( Clamp)

