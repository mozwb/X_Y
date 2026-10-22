# 测试夹具。断言暂时靠手跑；这里只负责把夹具编出来。
add_subdirectory(probe)
add_subdirectory(leaky)   # 反面夹具：故意不反注册，验证引擎能查出泄漏
