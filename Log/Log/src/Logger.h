#pragma once
#include<string>
#include<iostream>
#include"configure.h"
#include"core.h"
#include<vector>
namespace X_Y {
	using namespace  LogConfigur;
	using namespace  LogTools;
	using str = std::string;
	template<typename T> using vec = std::vector<T>;
	using std::cout;
	using std::cin;
	using std::endl;
	#define UN_KNOW  "Un_Know"
	
	template<typename T>
	class LogConfigure :public LogDevices {
	public:
		using Self = T;
		using LogDevices::logAll;
		using LogDevices::add;
		using LogDevices::remove;
		using LogDevices::clear;
		using LogDevices::toString;
		
		LOG_LEVEL_ALL(Self)
		LOG_ALL_API


		LogConfigure() {
			name = "DefaultLogger";
			this->add(new Console());
			Trace::setModel("%180:180:180%[位置][日期][时间][发起者][等级]:[内容]%#");
			Debug::setModel("%90:180:255%[位置][日期][时间][发起者][等级]:[内容]%#");
			Info::setModel("%80:220:100%[位置][日期][时间][发起者][等级]:[内容]%#");
			Warn::setModel("%255:210:0%[位置][日期][时间][发起者][等级]:[内容]%#");
			Error::setModel("%255:80:80%[位置][日期][时间][发起者][等级]:[内容]%#");
			Fatal::setModel("%220:0:0%[位置][日期][时间][发起者][等级]:[内容]%#");
		
		}
		LogConfigure(str name)
			: name(name)
		{
			this->add(new Console());
			Trace::setModel("%180:180:180%[位置][日期][时间][发起者][等级]:[内容]%#");
			Debug::setModel("%90:180:255%[位置][日期][时间][发起者][等级]:[内容]%#");
			Info::setModel("%80:220:100%[位置][日期][时间][发起者][等级]:[内容]%#");
			Warn::setModel("%255:210:0%[位置][日期][时间][发起者][等级]:[内容]%#");
			Error::setModel("%255:80:80%[位置][日期][时间][发起者][等级]:[内容]%#");
			Fatal::setModel("%220:0:0%[位置][日期][时间][发起者][等级]:[内容]%#");
		}
		virtual ~LogConfigure() {};
		void setname(str name) { this->name = name; }
		std::string toString() const {
			std::ostringstream oss;
			oss << "日志发起者: " << name << "\n";
			oss << "链接输出设备:" << LogDevices::toString();
			return oss.str();
		}
		template<typename LevelType, typename T, typename... Args>
		void log(T&& content, Args&&... args) {
			// 内部直接调用私有版本，传默认值
			Log<LevelType>(nullptr, -1, nullptr,
				std::forward<T>(content),
				std::forward<Args>(args)...);
		}
		template<typename LevelType, typename T, typename... Args>
		void Log(const char* file, int line, const char* func, T&& content, Args&&... args) {
			str lev = LevelType::toString();
			str model = LevelType::model;
			str result = replace(content, args...);
			str Mfile = file == nullptr ? UN_KNOW : To_Str(file);
			str MLine = line == -1 ? UN_KNOW : To_Str(line);
			str Mfunc = func == nullptr ? UN_KNOW : To_Str(func);

			result = format(model, lev, result, Mfile, MLine, Mfunc);
			logAll(result);
		}
	private:
		template<typename T, typename... Args>
		str replace(T&& content, Args&&... args) {
			std::string s = To_Str(std::forward<T>(content));
			vec<str> args_vec;
			//逗号运算符你敢信，它能依次执行，结果是最后表达式结果
			(args_vec.push_back(To_Str(std::forward<Args>(args))), ...);
			s = PiPei(s, args_vec);
			return s;
		}
		//如果你不喜欢这个格式化方式，可以重写这个函数，匹配你想要的模板（model）,可以用regex库
		virtual	str format(str mod, str lev, str content, str file, str line, str func) {
			replaceModel(mod, "[位置]", "[" + file + ":" + line + ":" + func + "]");
			replaceModel(mod, "[日期]", "[" + To_Str(DATE()) + "]");
			replaceModel(mod, "[时间]", "[" + To_Str(TIME()) + "]");
			replaceModel(mod, "[发起者]", "[" + name + "]");
			replaceModel(mod, "[等级]", "[" + lev + "]");
			replaceModel(mod, "[内容]", content);
			replaceColor(mod);
			return mod;
		}


		//如果你不喜欢用{}匹配参数，可以重写这个函数，可以用regex库
		virtual  str PiPei(str s, vec<str> args){
			for (auto arg : args) {
				replaceModel(s, '{', '}', arg);
			}
			replaceModel(s, "{{", "{");
			replaceModel(s, "}}", '}');
			return s;
		}

		friend std::ostream& operator<<(std::ostream& os, const LogConfigure<T>& log) {
			os << To_Str(log);
			return os;
		}

	public:
		str name;
	};
}

