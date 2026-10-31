#pragma once

/* 容器冒烟测试用服务契约。 */
typedef struct EchoService
{
    const char* (*say)(const char* text);
} EchoService;
