xy_module(HelloCmd SHARED DIR "${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(HelloCmd PRIVATE "${PROJECT_SOURCE_DIR}/Engine/include")
