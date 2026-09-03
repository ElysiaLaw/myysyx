#include "main.h"

Vtop* dut = NULL;
VerilatedVcdC* tfp = NULL;       // 波形文件对象
int main_time = 0;               // 仿真时间计数



int main(int arg,char* argc[]) 
{
	Verilated::commandArgs(arg, argc);
	Verilated::traceEverOn(true);


	dut = new Vtop;
	tfp = new VerilatedVcdC;

    	dut->trace(tfp, 99);                       //绑到模型,99=追踪深度
   	tfp->open("wave.vcd");                    //生成的文件名

	dut -> clk = 0;
	reset(5);
	
	for(int i=0;i<=5;i+=1)
	{
		dut->a =0;dut->b=0;
		single_cycle();

		dut->a =1;dut->b=0;
		single_cycle();

		dut->a =1;dut->b=1;
		single_cycle();

		dut->a =0;dut->b=1;
		single_cycle();
	}

	tfp->close();                              //结束关文件
	return 0;
}
