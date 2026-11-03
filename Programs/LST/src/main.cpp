#include "IO/Files.h"
#include "Core/Core.h"
#include <exception>
#include <iostream>
#include <string>
#include <utility>
//          路径 递归深度
//      LST (path)  （int)
//      默认从当前目录无限向下递归
//      递归深度>=0时，向下递归深度限制为depth
int main(int argc, char *argv[])
{
    X_Y::XPath currentDir = X_Y::XPath::CurrentDirectory();
    if (argc > 3)
    {
        std::cerr << "用法: LST [path] [depth]\n";
        return 2;
    }

    if (argc >= 2)
    {
        currentDir = X_Y::XPath(argv[1]);
    }

    if (argc < 3)
    {
        std::cout << currentDir.ToTree(currentDir.getName());
        return 0;
    }

    int depth = -1;
    try
    {
        depth = std::stoi(argv[2]);
    }
    catch (const std::exception &)
    {
        std::cerr << "depth 必须是整数\n";
        return 2;
    }
    if (depth < 0)
    {
        std::cerr << "depth 不能小于 0\n";
        return 2;
    }

    std::vector<std::string> files;
    std::vector<std::pair<X_Y::XPath, int>> dirs;
    dirs.emplace_back(currentDir, 0);
    while (!dirs.empty())
    {
        auto [dir, currentDepth] = dirs.back();
        dirs.pop_back();
        for (const X_Y::XPath &file : dir.ListXPaths())
        {
            if (file.IsDirectory())
            {
                if (currentDepth < depth)
                {
                    dirs.emplace_back(file, currentDepth + 1);
                }
            }
            else
            {
                files.push_back(file.getRelativePath(currentDir));
            }
        }
    }
    std::cout << X_Y::BuildTreeText(files, currentDir.getName());
    return 0;
}