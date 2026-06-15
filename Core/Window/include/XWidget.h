#pragma once
#include"Movement/include/movements.h"
#include"BaseWin.h"
#include<string>
#include<GraphicsContext/include/GraphicsContext.h>
#include<Log/include/XYLog.h>
namespace X_Y {
	
		using Base = BaseWin;
		using MovementType = X_Y::MovementType;
	
		class XWidget :protected Base {
		public :
			explicit XWidget(XWidget* parent=nullptr);
			~XWidget(){destroy();}

			//show中执行创建，才有窗口指针
			//除非主动调用create，否则应该show完之后在执行其他步骤
			bool show(showtype nShow=SHOW);
			void close();
			void destroy();
			//切断与父类相关的所有回调
			void disconnectPa();
			//让自己变成独立窗口
			void releaseSelf();

			void  Render() {
				XY_CORE_ASSERT(m_Context, "上下文不能为空");
				if (!m_Context->IsCurrent())
					if(!m_Context->MakeCurrent())
						XY_CORE_ASSERT(false, "MakeCurrent失败");
				this->onRender();
				m_Context->SwapBuffers();
			}

			virtual void onRender() {};
			std::string getname() { return toString();}
			std::string toString()const override{
				return m_title;
			}
			GraphicsContext* get_context() {
				return m_Context.get();
			}
			uint get_width() {
				return m_width;
			}
			uint get_height() {
				return m_height;
			}
			void setTitle(const char* title);
			void setSize(uint width, uint height);
			XWidget* getParent() const { return m_parent; }

			void SwapBuffers() {
				m_Context->SwapBuffers();
			}



			// C++11及以后：直接删除拷贝构造、拷贝赋值
			XWidget(const XWidget&) = delete;
			XWidget& operator=(const XWidget&) = delete;

			// 要不要移动看需求，也可以一起禁掉
			//XWidget(XWidget&&) = delete;
			//XWidget& operator=(XWidget&&) = delete;
			template<typename T = GraphicsContext,class... Args>
			void setGrContext(Args&& ... args) {
				XY_CORE_ASSERT((std::is_base_of_v<GraphicsContext,T>),"T must inherit from GraphicsContext")
				m_Context = CreateScope<T>(std::forward<Args>(args)...);
			}
			template<typename T = GraphicsContext>
			void createContext() {
				setGrContext<T>(this->GetNativeWindow());
				m_Context->Init();
			}
			bool create() {return this->Create(m_title, m_width, m_height);}
		private:
			XWidget* m_parent = nullptr;
			Scope<GraphicsContext> m_Context;
			const char* m_title = "X_Y";
			uint m_width = 800;
			uint m_height = 600;
		};
	}

