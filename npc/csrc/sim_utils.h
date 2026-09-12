#ifndef SIM_UTILS_H
#define SIM_UTILS_H

// 仿真工具依赖。
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "verilated.h"
#include "verilated_vcd_c.h"

// 模型类型与名字由 Makefile 传入，不能写死为 Vtop。
#define MAIN_TEXT_2(name) #name
#define MAIN_TEXT(name) MAIN_TEXT_2(name)
#define MAIN_MODEL_HEADER_2(name) <name.h>
#define MAIN_MODEL_HEADER(name) MAIN_MODEL_HEADER_2(name)
#include MAIN_MODEL_HEADER(TOP_CLASS)
#undef MAIN_MODEL_HEADER
#undef MAIN_MODEL_HEADER_2

// 模型信号与波形路径配置。
#ifndef MAIN_CLOCK_SIGNAL
#define MAIN_CLOCK_SIGNAL clk
#endif
#ifndef MAIN_RESET_SIGNAL
#define MAIN_RESET_SIGNAL rst
#endif
#ifndef MAIN_WAVE_PATH
#define MAIN_WAVE_PATH "wave.vcd"
#endif
// main 使用空实例名时，Verilator 顶层 IO 的 VPI 作用域。
#ifndef MAIN_VPI_SCOPE
#define MAIN_VPI_SCOPE "TOP"
#endif

// 主要变量由 main.cpp 定义；工具函数只引用。
extern TOP_CLASS* dut;
extern VerilatedVcdC* tfp;
extern uint64_t main_time;
extern int init_check;

// 时序工具：设置复位、产生正常时钟沿、推进逻辑时间。
void reset(bool active); // 仅写 rst；求值和时序由调用者负责
void clock_step();
void advance_time();
// 波形工具：注册检查位、记录当前时刻、整理显示层级。
void trace_init();
void trace_step();
bool hide_rootio_from_wave();
#endif
