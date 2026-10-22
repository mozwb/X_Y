# Math —— header-only；Glm/GLM.h 对外的就是 glm，所以把 glm 的 include 一起带出去。
xy_module(Math HEADER_ONLY)
target_link_libraries(Math INTERFACE xy_vendor_glm)
