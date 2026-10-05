# Tools and tests in this directory are built as part of the X_Y source tree.
add_executable(XYImageTool
    src/ImageTool.cpp)
target_link_libraries(XYImageTool PRIVATE XCore)

if(BUILD_TESTING)
    add_test(NAME XYImageDecoderSelfTest COMMAND XYImageTool --self-test)
endif()
