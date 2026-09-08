#include "main.h"

TOP_CLASS* dut = NULL;
VerilatedVcdC* tfp = NULL;       // 波形文件对象
int main_time = 0;               // 仿真时间计数
int init_check = 0;

int main(int argc, char* argv[])
{
    Verilated::commandArgs(argc, argv);
    // 使用空实例名，让 Verilator VPI 将顶层端口暴露为 TOP.<port>。
    dut = new TOP_CLASS("");

#ifdef TRACE_ON
    Verilated::traceEverOn(true);
    tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    trace_init();
    tfp->open("wave.vcd");
#endif

#if defined(TRACE_ON) && MAIN_HAS_STIM
    stim::Program program;
    std::vector<stim::Error> errors;

    if (!stim::load_program(program, errors))
    {
        stim::print_errors(errors);
#ifdef TRACE_ON
        tfp->close();
        delete tfp;
        tfp = NULL;
#endif
        delete dut;
        dut = NULL;
        return 1;
    }
#endif

#ifndef TRACE_ON
    nvboard_bind_all_pins(dut);
    nvboard_init();
#endif

    dut->clk = 0;
    reset(20);

#if defined(TRACE_ON) && MAIN_HAS_STIM
    if (!stim::init_runtime(errors))
    {
        stim::print_errors(errors);
        tfp->close();
        delete tfp;
        tfp = NULL;
        delete dut;
        dut = NULL;
        return 1;
    }
#endif

#ifdef TRACE_ON
    while (main_time <= MAX_TIME)
    {
        const uint64_t posedge_time =
            static_cast<uint64_t>(main_time);

#if MAIN_HAS_STIM
        stim::apply_inputs(posedge_time);
#endif
        myclock_time::half_single_posedge();
#if MAIN_HAS_STIM
        if (!stim::check_outputs(posedge_time))
        {
            init_check = 1;
        }
        else
        {
            init_check = 0;
        }
#endif
        trace_step();

        if (main_time > MAX_TIME)
        {
            break;
        }

        const uint64_t negedge_time =
            static_cast<uint64_t>(main_time);

#if MAIN_HAS_STIM
        stim::apply_inputs(negedge_time);
#endif
        myclock_time::half_single_negedge();
#if MAIN_HAS_STIM
        if (!stim::check_outputs(negedge_time))
        {
            init_check = 1;
        }
        else
        {
            init_check = 0;
        }
#endif
        trace_step();
    }
#else
    while (1)
    {
        nvboard_update();
        myclock_time::single_cycle();
    }
#endif

#ifndef TRACE_ON
    nvboard_quit();
#endif

#ifdef TRACE_ON
#if MAIN_HAS_STIM
    stim::finish_report();
#endif
    tfp->close();
    delete tfp;
    tfp = NULL;
    hide_rootio_from_wave();
#endif

    delete dut;
    dut = NULL;

#ifdef TRACE_ON
    return 0;
#else
    return 0;
#endif
}
