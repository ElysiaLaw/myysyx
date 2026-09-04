#include "myuse.h"

void single_cycle()
{
	single_posedge();
	single_negedge();
}


void single_posedge()
{
	dut -> clk = 1;
	dut -> eval();
#ifdef TRACE_ON
	tfp->dump(main_time ++);
#endif
}

void single_negedge()
{
	dut -> clk = 0;
	dut -> eval();
#ifdef TRACE_ON
	tfp->dump(main_time ++);
#endif
}

void reset(int n)
{
	while(n)
	{
		dut ->rst = 1;
		n -= 1;
#ifdef TRACE_ON
		tfp->dump(main_time++);
#endif
	}
	dut -> rst = 0 ;
#ifdef TRACE_ON
	tfp->dump(main_time++);
#endif
}
