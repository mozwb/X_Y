#include"xypch.h"
#include<Log/src/XYLog.h>
#include"Window/src/windows/XWidget.h"
#include"Window/src/Appliction.h"
int main(int argc, char* argv[]) {
	//logger.clear();
	std::cout << "aa" << X_Y::LOG::Info::getModel() << std::endl;
	//X_Y::LOG::Info::setModel("[等级] : [内容]");
	X_Y::Application app(argc,argv);
	X_Y::XWidget window("我的窗口", 800, 600);
	std::string l = "ll";
	X_Y::Console* con=new X_Y::Console(l);
 	logger.add(con);
	XINFO("hello");
	std::cout << "+66" << std::endl;
	window.show();
	std::cout << logger.toString() << std::endl;
	return app.exec();
}