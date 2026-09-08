#ifndef _MYUSE_H_
#define _MYUSE_H_

#include "main.h"


void reset(int n);
namespace myclock_time
{
	void single_cycle();
	void half_single_posedge();
	void half_single_negedge();
}

#endif
