#ifndef MAIN_H
#define MAIN_H

// 主仿真配置：编译命令传入的定义优先，缺失时使用这里的默认值。
#ifndef TRACE_ON
#define TRACE_ON 1
#endif
#ifndef MAX_TIME
#define MAX_TIME 1000
#endif
#ifndef RESET_TIME
#define RESET_TIME 20
#endif

// 主入口依赖；实现文件只包含 main.h。
#include <memory>
#include "sim_utils.h"
#if !TRACE_ON
#include <nvboard.h>
void nvboard_bind_all_pins(TOP_CLASS* top);
#endif

// STIM 是可选组件；移除它不影响主仿真和波形工具。
#if __has_include("stim.h")
#define MAIN_HAS_STIM 1
#include "stim.h"
#else
#define MAIN_HAS_STIM 0
#endif

// 留出末尾计数空间，避免时间自增溢出。
static_assert(MAX_TIME >= 0 && static_cast<uint64_t>(MAX_TIME) < UINT64_MAX,
              "MAX_TIME must be in [0, UINT64_MAX-1]");
static_assert(RESET_TIME >= 0 && static_cast<uint64_t>(RESET_TIME) < UINT64_MAX,
              "RESET_TIME must be in [0, UINT64_MAX-1]");
#endif
