#ifndef _MAIN_H_
#define _MAIN_H_

#include "myuse.h"

/**以下为可配置的默认参量*/

#ifndef TRACE_ON
#define TRACE_ON 1
#endif

/************************/

#ifndef MAX_TIME
#define MAX_TIME 1000
#endif


#define MAIN_TOP_HEADER_2(name) <name.h>
#define MAIN_TOP_HEADER(name) MAIN_TOP_HEADER_2(name)
#include MAIN_TOP_HEADER(TOP_CLASS)
#undef MAIN_TOP_HEADER
#undef MAIN_TOP_HEADER_2
#include "verilated.h"
#include <stdio.h>
#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include "verilated_vcd_c.h"
#include <nvboard.h>

// 只在 STIM 文件存在时接入 STIM；删除 stim.h/stim.cpp 后仍可独立编译 main。
#if defined(__has_include)
#if __has_include("stim.h")
#define MAIN_HAS_STIM 1
#include "stim.h"
#else
#define MAIN_HAS_STIM 0
#endif
#else
#define MAIN_HAS_STIM 0
#endif

extern TOP_CLASS* dut;
extern VerilatedVcdC* tfp;
extern int main_time;
extern int init_check;
void nvboard_bind_all_pins(TOP_CLASS* top);




#endif
