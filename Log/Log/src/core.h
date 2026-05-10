#pragma once
#include<iostream>
#include<string>
#include<regex>
#include"configure.h"
#include"core.h"
namespace LogTools {
    using namespace LogConfigur;
    using namespace LogTools;
    template <typename T, typename = void>
    struct HasToString : std::false_type {};

    template <typename T>
    struct HasToString<T, std::void_t<decltype(std::declval<T>().toString())>> : std::true_type {};


    template<typename T, typename = void>
    struct HasStaticToString : std::false_type {};

    template<typename T>
    struct HasStaticToString<T, std::void_t<decltype(T::toString())>>
        : std::true_type {
    };
    template <typename T, typename = void>
    struct IsStreamable : std::false_type {};

    template <typename T>
    struct IsStreamable<T, std::void_t<decltype(std::declval<std::ostringstream&>() << std::declval<T>())>>
        : std::true_type {
    };

    template <class T>
    inline static std::string To_Str(const T& obj) {

        if constexpr (std::is_pointer_v<T>) {
            if (obj == nullptr) return "nullptr";
            if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>, char>) {
                return std::string(obj);
            }
            return To_Str(*obj); // 解引用，递归调用！
        }


        if constexpr (HasToString<T>::value) {
            return obj.toString();
        }
        else if constexpr(HasStaticToString<T>::value){
            return T::toString();
        }
        else if constexpr (IsStreamable<T>::value) {
            std::ostringstream oss;
            oss << obj; // 只要类型支持 operator<<，就能直接输出
            return oss.str();
        }
        else {
			return "NO_TO_STRING";
        }
    }

    template <typename T>
    struct IsCustomLogBase : std::is_base_of<CustomLogBase , T> {};
    
    template<class T>
    requires IsCustomLogBase<T>::value  // 【外部约束！最直观！】
    inline std::ostream& operator<<(std::ostream& os,const T& obj)
    {
        if constexpr (HasToString<T>::value|| HasStaticToString<T>::value) {
            return os << To_Str<T>(obj);
        }
        else {
            return os << "NO_TO_STRING";
        }
    }
    template<typename T, typename... Args>
    inline static void replaceModel(std::string& content, T&& model, Args&& ... args) {
        std::string mod = To_Str(std::forward<T>(model));
        //逗号运算符你敢信，它能依次执行，结果是最后表达式结果
        (
            //lambda表达式
            [&](auto&& arg)//参数
            {
            size_t l = content.find(mod);
            if (l != std::string::npos) {
                content.replace(l, mod.size(), To_Str(arg));
                }
            }//函数体
            (std::forward<Args>(args)),//调用 
          ...);//这里报错可能是编译器犯傻，但能编译运行
        };
    template<typename T, typename... Args>
    inline static void replaceModel(std::string& content, T&& le, T&& re, Args&&... args)
    {
        std::string left = To_Str(std::forward<T>(le));
        std::string right = To_Str(std::forward<T>(re));
        std::string cleft = left + left;   // {{
        std::string cright = right + right; // }}

        size_t pos = 0;

        // 正确遍历参数，一个一个替换
        ([&](auto&& arg) {
            while (pos < content.size())
            {
                // 先找 {{
                size_t check_double = content.find(cleft, pos);
                size_t l = content.find(left, pos);

                // 如果是 {{ ，跳过！
                if (check_double != std::string::npos && check_double == l)
                {
                    pos = l + 2;
                    continue;
                }

                // 找匹配的 }
                size_t r = content.find(right, l);
                if (l == std::string::npos || r == std::string::npos) break;

                // 正常替换
                content.replace(l, r - l + 1, To_Str(arg));
                pos = r + 1;
                break;
            }
            }(std::forward<Args>(args)), ...);
    }
    //下面实现颜色替换
    // \033[38;2;R;G;Bm 要显示的文字 \033[0m
    //\033[48; 2; 30; 30; 30m 改变背景\033[0m\n
	//呜呜才发现能用正则，不过要注意转义，%和#都要转义成%%和##，否则正则会把它当成特殊字符
    inline static void processColorEscape(std::string& res) {
        res = std::regex_replace(res, std::regex(R"(%%)"), "%");
        res = std::regex_replace(res, std::regex(R"(##)"), "#");
    }
    inline static  void setColorChar(std::string& res) {
        std::regex reg(R"((^|[^%])%(\d+):(\d+):(\d+)%)");
        std::smatch match;
        res = std::regex_replace(
            res,
            reg,
            "$1\x1B[38;2;$2;$3;$4m"  
        );
    }
    inline static  void setColorBgd(std::string& res) {
        std::regex reg(R"((^|[^#])#(\d+):(\d+):(\d+)#)");
        std::smatch match;
        res = std::regex_replace(
            res,
            reg,
            "$1\x1B[48;2;$2;$3;$4m"
        );
    }
    inline static  void setColorEnd(std::string& res) {
        std::regex reg(R"((^|[^%])%#)");
        std::smatch match;
        res = std::regex_replace(
            res,
            reg,
            "$1\x1B[0m"
        );
    }
    inline static void replaceColor(std::string& result){
        setColorChar(result);
        setColorBgd(result);
        setColorEnd(result);
        processColorEscape(result);

    }
}