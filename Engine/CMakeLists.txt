# Engine —— 引擎本体。构建器（xy）将来也在这一层，所以都放这个目录。
#
# X_Y.exe 目前只做：解析参数 → 载入程序 DLL → 调入口 → 退出。
# EXPORTS 打开 ENABLE_EXPORTS：模块 DLL 将来要反向链这个 exe 的 import lib，
# 这样进程内只有一份 XCore / 分配器 / RTTI。

xy_program(X_Y
    DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    EXPORTS)

target_include_directories(X_Y PRIVATE include)

if(WIN32)
    target_link_libraries(X_Y PRIVATE kernel32)
else()
    target_link_libraries(X_Y PRIVATE dl)
endif()
