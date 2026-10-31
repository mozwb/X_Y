#include "../Process.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace X_Y::Process
{
    namespace
    {
        std::string ErrnoText(int error)
        {
            return std::string(std::strerror(error));
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

        int pipefd[2] = {-1, -1};
        if (options.Capture)
        {
            if (::pipe(pipefd) != 0)
            {
                result.Error = "pipe 失败: " + ErrnoText(errno);
                return result;
            }
        }

        const pid_t pid = ::fork();
        if (pid < 0)
        {
            result.Error = "fork 失败: " + ErrnoText(errno);
            if (options.Capture)
            {
                ::close(pipefd[0]);
                ::close(pipefd[1]);
            }
            return result;
        }

        if (pid == 0)
        {
            // 子进程：stdout / stderr 合并到同一根管道。
            if (options.Capture)
            {
                ::close(pipefd[0]);
                if (::dup2(pipefd[1], STDOUT_FILENO) < 0 ||
                    ::dup2(pipefd[1], STDERR_FILENO) < 0)
                {
                    ::_exit(127);
                }
                ::close(pipefd[1]);
            }

            const std::string programString(program);
            std::vector<char*> argv;
            argv.reserve(arguments.size() + 2);
            argv.push_back(const_cast<char*>(programString.c_str()));
            for (const auto& argument : arguments)
            {
                argv.push_back(const_cast<char*>(argument.c_str()));
            }
            argv.push_back(nullptr);

            ::execvp(programString.c_str(), argv.data());
            ::_exit(127); // exec 失败，父进程会看到 127
        }

        result.Started = true;

        if (options.Capture)
        {
            ::close(pipefd[1]);

            char buffer[4096];
            for (;;)
            {
                const ssize_t bytes = ::read(pipefd[0], buffer, sizeof(buffer));
                if (bytes > 0)
                {
                    result.Output.append(buffer, static_cast<std::size_t>(bytes));
                }
                else if (bytes == 0)
                {
                    break;
                }
                else if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    break;
                }
            }
            ::close(pipefd[0]);
        }

        int status = 0;
        while (::waitpid(pid, &status, 0) < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            result.Error = "waitpid 失败: " + ErrnoText(errno);
            result.ExitCode = -1;
            return result;
        }

        if (WIFEXITED(status))
        {
            result.ExitCode = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status))
        {
            result.ExitCode = 128 + WTERMSIG(status);
        }
        else
        {
            result.ExitCode = -1;
        }

        return result;
    }
}
