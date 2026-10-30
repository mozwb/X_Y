# X_Y

## 设计图
![截图](./X_Y设计图.jpg)

## 核心设计

- Engine:
1. 运行器：Engine的第一方面作用就是运行嵌入在其上的指令或者程序。可以将指令或者程序放在dll里也可以直接挂起一个可执行程序。
2. 内核器：同时将内存管理统计收束到这里，或者读写之类的操作，在这里可以做安全防护

- Application:
- Engine上所运行的程序。虽然我希望Engine可以支持运行不同的程序，但有时候把Engine和APP当作一个完整的产品也不错，我认为这样子做出来的产品具有双核结构（Engine and APP），可以将底层指令和业务指令分别放在两个核心。


- Engine Container:
- 为了方便拓展，我觉得Engine运行的第一个核心应该是一个承载伪Engine的容器，具有类似于Engine的能力但是权能小于其。同时每个伪Engine可以加载自己的指令集，让Engine Container记录每个伪Engine的名字，然后内核Engine可以通过名字去运行某个伪Engine的指令。

- Cpython
- 这样子的话我需要一种更方便得脚本语言我觉得python就不错。但是又不同于Engine的命令，Engine的命令是相当于函数一样的，不存在变量，然后我们只需要让pyhton能够调用Engine，这样子就能跑我们给Engine的命令。然后我们让Cpython的内存分配通知一下我们的Engine

- 构建器
-  可以收集源码进行编译，然后热加载到Engine里，可以是伪Engine第一插入的命令集,摒弃CMake，自己收集源码构建