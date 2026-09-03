#include "myuse.h"

void single_cycle()
{
	dut -> clk = 1;
	dut -> eval();
	tfp->dump(main_time ++);
	dut -> clk = 0;
	dut -> eval();
	tfp->dump(main_time ++);
}

void reset(int n)
{
	while(n)
	{
		dut ->rst = 1;
		n -= 1;
		tfp->dump(main_time++);
	}
	dut -> rst = 0 ;
	tfp->dump(main_time++);
}
