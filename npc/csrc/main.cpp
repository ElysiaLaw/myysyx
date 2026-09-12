#include "main.h"

TOP_CLASS* dut = nullptr;
VerilatedVcdC* tfp = nullptr;
uint64_t main_time = 0;
int init_check = 0;

int main(int argc, char* argv[])
{
    Verilated::commandArgs(argc, argv);
    uint64_t max_time = MAX_TIME;
    uint64_t reset_time = RESET_TIME;
    // 模型先于 Program 创建，保证 VPI 句柄先于模型释放。
    auto model = std::make_unique<TOP_CLASS>("");
    dut = model.get();

#if MAIN_HAS_STIM
    stim::Program program;
    const char* script = argc > 1 ? argv[1] : STIM_FILE_PATH;
    const bool loaded =
        program.load(script, MAIN_TEXT(MY_TOP), MAIN_VPI_SCOPE, MAIN_TEXT(MAIN_CLOCK_SIGNAL),
                     MAIN_TEXT(MAIN_RESET_SIGNAL), !TRACE_ON);
#if TRACE_ON
    if (!loaded)
    {
        return 1;
    }
    if (program.config().max_time)
    {
        max_time = *program.config().max_time;
    }
#endif
    if (loaded && program.config().reset_time)
    {
        reset_time = *program.config().reset_time;
    }
#endif

#if TRACE_ON
    Verilated::traceEverOn(true);
    auto wave = std::make_unique<VerilatedVcdC>();
    tfp = wave.get();
    dut->trace(tfp, 99);
    trace_init();
    tfp->open(MAIN_WAVE_PATH);
    if (!tfp->isOpen())
    {
        std::cerr << "SIM ERROR: cannot open " << MAIN_WAVE_PATH << '\n';
        return 1;
    }
    if (max_time < reset_time)
    {
        std::cerr << "SIM WARNING: MAX_TIME precedes reset release; simulation ends during reset\n";
    }
#else
    nvboard_bind_all_pins(dut);
    nvboard_init();
#endif

    // 初始时刻只求值；后续每个刻度按同一规则翻转时钟。
    dut->MAIN_CLOCK_SIGNAL = 0;
    reset(reset_time != 0);
    for (;;)
    {
#if !TRACE_ON
        nvboard_update();
#endif
#if TRACE_ON && MAIN_HAS_STIM
        if (main_time > reset_time)
        {
            program.apply(main_time);
        }
#endif
        if (main_time != 0)
        {
            clock_step();
        }
        else
        {
            dut->eval();
        }

        // 先完成本刻度的正常边沿，再释放复位，不增加额外时间。
        if (main_time == reset_time)
        {
            reset(false);
#if TRACE_ON && MAIN_HAS_STIM
            program.start(reset_time);
#endif
            dut->eval();
        }
        // 模型求值后检查，确保波形中的标记属于当前时刻。
        init_check = 0;
#if TRACE_ON && MAIN_HAS_STIM
        if (main_time > reset_time)
        {
            init_check = program.check(main_time) ? 0 : 1;
        }
#endif
        trace_step();
#if TRACE_ON
        if (main_time == max_time || Verilated::gotFinish())
        {
            break;
        }
#else
        if (Verilated::gotFinish())
        {
            break;
        }
#endif
        advance_time();
    }

    dut->final();
#if TRACE_ON
    tfp->close();
    wave.reset();
    tfp = nullptr;
    const bool wave_ok = hide_rootio_from_wave();
#else
    nvboard_quit();
#endif
    dut = nullptr;
#if TRACE_ON
    return wave_ok ? 0 : 1;
#else
    return 0;
#endif
}
