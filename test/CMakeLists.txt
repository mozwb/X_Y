# Tools and tests in this directory are built as part of the X_Y source tree.
add_executable(XYImageTool
    src/ImageTool.cpp)
target_link_libraries(XYImageTool PRIVATE UI)

add_executable(XYVisualTest
    src/main.cpp)
target_link_libraries(XYVisualTest PRIVATE UI)

if(BUILD_TESTING)
    add_test(NAME XYImageDecoderSelfTest COMMAND XYImageTool --self-test)
endif()
