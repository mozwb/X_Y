#include <iostream>
#include "Log.h"

class Student {
public:
    std::string toString() const {
        return "Student{ name=张三, age=18 }";
    }
};
//using namespace LogTools;
struct op :public X_Y::LogConfigure<op> {
    using X_Y::LogConfigure<op>::LogConfigure;
    LOG_LEVEL_EXT(oh, op)
    LOG_LEVEL_API(oh)
        // 2. 写一个【完美转发构造】，既能接收父类构造，又能跑你的代码
        template <typename... Args>
    op(Args&&... args)
        : LogConfigure<op>(std::forward<Args>(args)...) // 调用父类构造
    {
        // 你的自定义代码！！！
        oh::setModel("%90:180:255%[日期][时间][发起者][等级]:[内容]%#");
    }
};


int main() {
    X_Y::LogConfigure<void> test("test");

   

    // 测试字符串
    test.log<X_Y::LogConfigure<void>::Info>("hello MY_LOG");
    test.logInfo("000");

    test.log<X_Y::LogConfigure<void>::Info>(test);


	test.logError("颜色变了吗？");
        
	test.add(new X_Y::File("log.txt"));
	test.log<X_Y::LogConfigure<void>::Error>("该日志应该同时出现在控制台和log.txt里");

    std::cout << test << std::endl;

    X_Y::LOG app("ll");
	app.Log<X_Y::LOG::Debug>(__FILE__, __LINE__, __FUNCTION__,"这是一个调试日志，应该有不同的颜色");
	X_Y::LOG::Debug::setModel("%90:180:255%[日期][时间][发起者][等级]:[内容]%#");
    log(app, Debug, "这是一个调试日志，应该有不同的颜色");
    op mytest;
	op mylog("MyApp");
    std::cout << mytest << std::endl;
	mytest.logoh("这是一个oh日志，应该有不同的颜色");
    std::cout << mylog << std::endl;
	mylog.logoh("这是一个oh日志，应该有不同的颜色");


    LOG(mylog, Debug, __FILE__, __LINE__, __FUNCTION__, "这是一个调试日志，应该有不同的颜色");
    return 0;
}