# DataStore —— 键值存储，落在文件系统上。
#
# 定位：**工具侧模块**（将来搬去 Modules 仓库、编成 dll 由引擎装载）。
# 现在还不能切 SHARED：它依赖 Files/Memory 这两个内建，切了就会把内建各静态链一份
# 到 dll 里（全局计数器/分配器各一份）。等引擎导出内建符号（或走 host 服务）之后再切。

if(NOT TARGET Files)
    message(STATUS "DataStore: 依赖 Files，而 Files 在本平台被跳过 → 一并跳过")
    return()
endif()

xy_module(DataStore DEPS Memory Files)
