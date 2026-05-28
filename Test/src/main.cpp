#include"xypch.h"
#include<Log/src/XYLog.h>
#include"Window/src/windows/XWidget.h"

#include"Window/src/Application.h"
int main(int argc, char* argv[]) {
	X_Y::Application app(argc,argv);
	X_Y::XWidget window;
	X_Y::XWidget childern(&window) ;
	window.show();
	childern.setTitle("children");
	childern.show();
	while (app.isRunning()) {
		app.pushEvents();
		app.ProcessEvents();
	}
	return 0;
}