# X_Y Linux 验收清单

> 2026-10-31：用户已在 Linux 机器上跑过验收，服务器随后关闭；验收输出未留档。
> 下面保留原始清单，供以后复跑 / 新环境参考。

> 本机是 Windows，`ProcessPosix` / `LibraryPosix` 只做过语法桩检查，还没跑过真的 `dlopen` / `fork+exec`。
> 这份清单给 Linux 机器用；跑完把输出贴回来，我按结果修。

## 0. 准备

需要：

- `g++`（支持 C++20）
- `cmake`（>= 3.24）
- `ninja`
- 工作区里同时有 `X_Y/` 和 `Modules/`

```bash
export WORKSPACE=$HOME/workbench
cd "$WORKSPACE"
```

## 1. 先编 Memory（Linux 版）

`X_Y` 的 CMake 现在通过 `XY_MEMORY_LIB` 链接一个**已经编好的** `libMemory.a`。Linux 上先编：

```bash
cmake -S Modules -B Modules/build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DXY_MODULES=Memory

cmake --build Modules/build-linux --target Memory
```

期望产物：

```
Modules/build-linux/out/lib/libMemory.a
```

## 2. 编 X_Y

> 输出布局已统一：`out/bin` 放所有可运行/可加载的东西（`X_Y`、`xy`、`HelloXy.so`、`pkg.so`），
> `out/lib` 只放编译期库。之前编过旧布局（`.so` 在 `out/lib`）的话，先 `rm -rf X_Y/build-linux` 重来。

```bash
cmake -S X_Y -B X_Y/build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DXY_MEMORY_LIB="$WORKSPACE/Modules/build-linux/out/lib/libMemory.a"

cmake --build X_Y/build-linux
```

期望产物：

```
X_Y/build-linux/out/bin/X_Y
X_Y/build-linux/out/bin/xy
X_Y/build-linux/out/bin/HelloXy.so
X_Y/build-linux/out/bin/pkg.so
```

## 3. 内核冒烟

```bash
cd "$WORKSPACE/X_Y"

./build-linux/out/bin/X_Y --version
./build-linux/out/bin/X_Y --run abi
./build-linux/out/bin/X_Y --run mem
```

期望：

```
X_Y  abi=1  host=64 bytes
abi=1 host=64 bytes program=xy_program_v1 commands=xy_commands_v1
门面: ...
进程: ...
```

## 4. 外部进程（Linux fork/exec）

```bash
./build-linux/out/bin/X_Y --run exec --capture sh -c "echo hello"
./build-linux/out/bin/X_Y --run exec sh -c "exit 7"
echo "exit=$?"
```

期望：

```
hello
exit=7
```

## 5. 微程序

CMake 编出来的是 `HelloXy.so`：

```bash
./build-linux/out/bin/X_Y --program ./build-linux/out/bin/HelloXy.so a b
```

期望：

```
HelloXy
  abi   = 1
  host  = 64 bytes
  argc  = 3
  argv[0] = HelloXy
  argv[1] = a
  argv[2] = b
  alloc = 0x...
```

## 6. pkg

```bash
./build-linux/out/bin/X_Y --module ./build-linux/out/bin/pkg.so --run pkg list --repo ./Modules
./build-linux/out/bin/X_Y --module ./build-linux/out/bin/pkg.so --run pkg verify --repo ./Modules
```

如果 `Modules` 里还没有 `package.xy`，`list` 会显示“没有找到 package.xy”，这是正常的。

固定布局下，`X_Y` 启动时会把 `XY_MODULES` 设成 `out/bin/../modules`（也就是 `out/modules`）。
不带 `--root` 时，`pkg list` / `pkg install` 默认就用这个目录；`xy install` 的默认 root 也是它。

## 7. xy build / install

```bash
./build-linux/out/bin/xy doctor --repo ./Modules

./build-linux/out/bin/xy build Programs/HelloXy
```

期望：

```
built HelloXy 0.1.0 -> .cache/out/HelloXy/0.1.0/HelloXy.dll
```

注意：manifest 里 `file = HelloXy.dll`，Linux 上也会产出这个文件名；`dlopen` 不关心扩展名，能加载。

```bash
./build-linux/out/bin/X_Y --program .cache/out/HelloXy/0.1.0/HelloXy.dll
```

再用 pkg 收进临时目录：

```bash
rm -rf /tmp/xy-install
./build-linux/out/bin/X_Y --module ./build-linux/out/bin/pkg.so \
  --run pkg install .cache/out/HelloXy/0.1.0 --root /tmp/xy-install

./build-linux/out/bin/X_Y --module ./build-linux/out/bin/pkg.so \
  --run pkg verify /tmp/xy-install
```

期望：

```
[OK]   HelloXy 0.1.0
```

## 8. 已知 / 待确认

1. `ProcessPosix` 的 `execvp` 失败会以 127 退出，`Error` 为空；这是 v1 的简化。
2. `ProcessPosix` 的 `fork` 目前是单线程进程里做的；以后内核上线程要换 `posix_spawn` 或保证 fork 后只做 async-signal-safe 的事。
3. `X_Y` 链 `libMemory.a` 用的是绝对路径；如果换机器，重新 configure 时改 `XY_MEMORY_LIB`。
4. `HelloXy` 的 manifest 固定 `file = HelloXy.dll`；CMake 产物是 `.so`。两者都能被 `dlopen`，但看起来不统一，后面可以按平台写 `file_linux` / `file_windows`。
5. shell 在非 tty（管道）里也会打 `> `，v1 已知。
6. `pkg` 的 `--repo` 和位置参数 root 同时给时，后面的覆盖前面的。

## 9. 需要贴回来的

- `g++ --version`、`cmake --version`、`uname -a`
- 第 2 步的 configure / build 结果
- 第 3–7 步每一条的命令和输出；失败就贴完整报错
