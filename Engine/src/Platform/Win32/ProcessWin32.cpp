#include "../Process.h"

#include <windows.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace X_Y::Process
{
    namespace
    {
        std::wstring Utf8ToWide(std::string_view text)
        {
            if (text.empty())
            {
                return {};
            }

            const int length = static_cast<int>(text.size());
            const int wideLength = ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, nullptr, 0);
            if (wideLength <= 0)
            {
                return {};
            }

            std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
            if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                      text.data(), length,
                                      wide.data(), wideLength) != wideLength)
            {
                return {};
            }
            return wide;
        }

        // Windows 命令行转义：反斜杠只在引号前和结尾翻倍。
        void AppendQuoted(std::wstring& out, const std::wstring& arg)
        {
            if (!out.empty())
            {
                out.push_back(L' ');
            }

            const bool needQuotes =
                arg.empty() ||
                arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
            if (!needQuotes)
            {
                out += arg;
                return;
            }

            out.push_back(L'"');
            std::size_t i = 0;
            while (i < arg.size())
            {
                std::size_t backslashes = 0;
                while (i < arg.size() && arg[i] == L'\\')
                {
                    ++i;
                    ++backslashes;
                }

                if (i == arg.size())
                {
                    out.append(backslashes * 2, L'\\');
                    break;
                }

                if (arg[i] == L'"')
                {
                    out.append(backslashes * 2 + 1, L'\\');
                    out.push_back(L'"');
                }
                else
                {
                    out.append(backslashes, L'\\');
                    out.push_back(arg[i]);
                }
                ++i;
            }
            out.push_back(L'"');
        }

        std::string ReadAll(HANDLE pipe)
        {
            std::string out;
            char buffer[4096];
            DWORD read = 0;

            while (::ReadFile(pipe, buffer, static_cast<DWORD>(sizeof(buffer)),
                              &read, nullptr) &&
                   read > 0)
            {
                out.append(buffer, read);
            }
            return out;
        }
    }

    Result Run(std::string_view program,
               const std::vector<std::string>& arguments,
               Options options)
    {
        Result result;

        if (program.empty())
        {
            result.Error = "程序名为空";
            return result;
        }

        const std::wstring wideProgram = Utf8ToWide(program);
        if (wideProgram.empty())
        {
            result.Error = "程序名不是合法 UTF-8";
            return result;
        }

        std::wstring commandLine;
        AppendQuoted(commandLine, wideProgram);
        for (const auto& argument : arguments)
        {
            const std::wstring wideArgument = Utf8ToWide(argument);
            if (wideArgument.empty() && !argument.empty())
            {
                result.Error = "参数不是合法 UTF-8: " + argument;
                return result;
            }
            AppendQuoted(commandLine, wideArgument);
        }

        SECURITY_ATTRIBUTES attributes{};
        attributes.nLength = sizeof(attributes);
        attributes.bInheritHandle = TRUE;

        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;

        if (options.Capture)
        {
            if (!::CreatePipe(&readPipe, &writePipe, &attributes, 0))
            {
                result.Error = "CreatePipe 失败，错误码 " +
                               std::to_string(static_cast<unsigned long>(::GetLastError()));
                return result;
            }
            if (!::SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0))
            {
                result.Error = "SetHandleInformation 失败，错误码 " +
                               std::to_string(static_cast<unsigned long>(::GetLastError()));
                ::CloseHandle(readPipe);
                ::CloseHandle(writePipe);
                return result;
            }
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        if (options.Capture)
        {
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
            startup.hStdOutput = writePipe;
            startup.hStdError = writePipe;
        }

        PROCESS_INFORMATION process{};
        std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
        mutableCommandLine.push_back(L'\0');

        const BOOL created = ::CreateProcessW(
            nullptr,
            mutableCommandLine.data(),
            nullptr,
            nullptr,
            options.Capture ? TRUE : FALSE,
            0,
            nullptr,
            nullptr,
            &startup,
            &process);

        if (!created)
        {
            result.Error = "CreateProcessW 失败，错误码 " +
                           std::to_string(static_cast<unsigned long>(::GetLastError()));
            if (readPipe != nullptr)
            {
                ::CloseHandle(readPipe);
            }
            if (writePipe != nullptr)
            {
                ::CloseHandle(writePipe);
            }
            return result;
        }

        if (writePipe != nullptr)
        {
            ::CloseHandle(writePipe);
            writePipe = nullptr;
        }

        result.Started = true;

        if (options.Capture)
        {
            result.Output = ReadAll(readPipe);
            ::CloseHandle(readPipe);
            readPipe = nullptr;
        }

        ::WaitForSingleObject(process.hProcess, INFINITE);

        DWORD exitCode = 0;
        if (!::GetExitCodeProcess(process.hProcess, &exitCode))
        {
            result.Error = "GetExitCodeProcess 失败，错误码 " +
                           std::to_string(static_cast<unsigned long>(::GetLastError()));
        }
        else
        {
            result.ExitCode = static_cast<int>(exitCode);
        }

        ::CloseHandle(process.hThread);
        ::CloseHandle(process.hProcess);
        return result;
    }
}
