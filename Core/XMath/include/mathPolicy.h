#pragma once
namespace X_Y {
		struct MathPolicy {};
}
#define MATH_VOID(name, type) using name = type;

#define MATH_FUNC(funcName, targetNamespace) \
static decltype(auto) funcName(auto&&... args) \
{ \
    return targetNamespace(std::forward<decltype(args)>(args)...); \
}



//如果Type里定义的函数名和自己定义的函数名一样，就直接用MATH_NAME_USE，
#define MATH_NAME_USE(name) using name = typename Type::name;
//self是自己定义的名字，name是Type里定义的名字
#define MATH_SELF_NAME_USE(self,name) using self = typename Type::name;
//如果Type里定义的函数名和自己定义的函数名一样，就直接用MATH_FUNC，如果不一样就用MATH_SELF_API
#define MATH_API(FuncName) \
  static decltype(auto) FuncName(auto&&... args) \
  { \
    return Type::FuncName( \
      std::forward<decltype(args)>(args)... \
    ); \
  }
//self是自己定义的名字，FuncName是Type里定义的名字
#define MATH_SELF_API(SelfName,FuncName) \
  static decltype(auto) SelfName(auto&&... args) \
  { \
    return Type::FuncName( \
      std::forward<decltype(args)>(args)... \
    ); \
  }