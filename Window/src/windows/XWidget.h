#pragma once
#include"WinCore.h"
#include<string>
namespace X_Y {
		typedef unsigned int uint;
		using Base = WinCore::BaseWin;

		class XWidget :protected Base {
		public :
			XWidget(const char* title="X_Y", uint width = 800, uint height = 600);
			~XWidget(){destroy();}
			void show(int nShow = SW_SHOW);
			void close();
			void destroy();

			std::string getname() { return toString();}
			std::string toString()const{
				return m_title;
			}
			//HWND handle() const { return GetHwnd(); }
		private:
			const char* m_title;
			uint m_width;
			uint m_height;
			bool create(const char* title, uint width, uint height);
		};
	}

