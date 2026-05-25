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
			XWidget(const char* title="X_Y", uint width = 800, uint height = 600);
			~XWidget(){destroy();}
			bool show(showtype nShow=SHOW);
			void close();
			void destroy();

			std::string getname() { return toString();}
			std::string toString()const override{
				return m_title;
			}
		private:
			const char* m_title;
			uint m_width;
			uint m_height;
			bool create(const char* title, uint width, uint height);
		};
	}

