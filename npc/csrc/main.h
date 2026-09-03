#ifndef _MAIN_H_
#define _MAIN_H_

#include "myuse.h"
#include "Vtop.h"
#include "verilated.h"
#include <stdio.h>
#include "verilated_vcd_c.h"
#include <nvboard.h>


//#define TRACE_ON
#define MAX_TIME 1000

extern Vtop* dut;
extern VerilatedVcdC* tfp;
extern int main_time;




#endif
