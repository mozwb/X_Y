#pragma once
#include"WinCore.h"
#include<string>
namespace X_Y {
		typedef unsigned int uint;
		using Base = BaseWin;
		using X_Y::MovementType;
		enum showtype{
			HIDE,
			NORMAL,
			MINIMIZED,
			MAXIMIZED,
			NOACTIVATE,
			SHOW,
			MINIMIZE,
			MINNOACTIVE,
			SHOWNA,
			RESTORE,
			DEFAULT
		};
		class XWidget :protected Base {
		public :
			explicit XWidget(XWidget* parent=nullptr);
			~XWidget(){destroy();}
			bool show(showtype nShow=SHOW);
			void close();
			void destroy();
			//切断与父类相关的所有回调
			void disconnectPa();
			//让自己变成独立窗口
			void releaseSelf();
			std::string getname() { return toString();}
			std::string toString()const override{
				return m_title;
			}
			void setTitle(const char* title);
			void setSize(uint width, uint height);
			XWidget* getParent() const { return m_parent; }

			// C++11及以后：直接删除拷贝构造、拷贝赋值
			XWidget(const XWidget&) = delete;
			XWidget& operator=(const XWidget&) = delete;

			// 要不要移动看需求，也可以一起禁掉
			//XWidget(XWidget&&) = delete;
			//XWidget& operator=(XWidget&&) = delete;

		private:
			const char* m_title = "X_Y";
			uint m_width = 800;
			uint m_height = 600;
			XWidget* m_parent;
			virtual bool create(const char* title, uint width, uint height);
		};
	}

