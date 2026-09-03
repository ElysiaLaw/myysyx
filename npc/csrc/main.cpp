#include "main.h"

Vtop* dut = NULL;
VerilatedVcdC* tfp = NULL;       // 波形文件对象
int main_time = 0;               // 仿真时间计数

void nvboard_bind_all_pins(Vtop* top);

int main(int arg,char* argc[]) 
{
	Verilated::commandArgs(arg, argc);
	dut = new Vtop;

#ifdef TRACE_ON
	Verilated::traceEverOn(true);
	tfp = new VerilatedVcdC;
    	dut->trace(tfp, 99);
   	tfp->open("wave.vcd");
#endif

	nvboard_bind_all_pins(dut);
	nvboard_init();

	dut -> clk = 0;
	reset(20);

#ifdef TRACE_ON
	for(int i=0;i<=MAX_TIME;i+=1)
#endif
#ifndef TRACE_ON
	while(1)
#endif
	{
		nvboard_update();
		single_cycle();
	}

	nvboard_quit();
#ifdef TRACE_ON
	tfp->close();                              //结束关文件
#endif
	return 0;
}
