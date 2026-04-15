#pragma once
#include"Core.h"
namespace Hazel {
	//当由用户继承并创建
	class HAZEL_API Application
	{
	public:
		Application();
		virtual ~Application();
		void Run();
	};
	//由用户定义的函数
	Application* createApplication();
}