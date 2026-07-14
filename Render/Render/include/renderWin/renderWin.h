#pragma once
#include"Widget/include/XWidget.h"
#include"Render/include/renderEvent/RenderEvent.h"
#include"Render/include/Render.h"
#include"Application/include/Application.h"

//为啥不把这个类放在Xwidget?因为我觉得我可能会用XWidget去做一些不需要渲染的窗口，这样子的话这个RenderWin对于哪个项目是不必要的存在
//我比较喜欢qt的窗口管理，感觉这些窗口像是继承后再开发的
//普通窗口不需要实现发送渲染事件，只有需要渲染的窗口才需要实现这个类，这样子的话就不会有不必要的函数存在了
namespace X_Y {
	class RenderWin :public XWidget {
	public:
		RenderWin(XWidget* parent=nullptr);
		~RenderWin() = default;
		void RenderInit();
		//发送消息给Render
		void emit(RenderEvent* e);
	};
}