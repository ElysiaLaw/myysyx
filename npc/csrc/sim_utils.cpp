#include "sim_utils.h"

namespace
{
    struct InitCheckTrace
    {
        uint32_t code = 0;
        CData value = 0;
    };

    InitCheckTrace init_check_trace;

    void init_check_init_callback(void* user, VerilatedVcd* trace, uint32_t code)
    {
        InitCheckTrace* state = static_cast<InitCheckTrace*>(user);
        state->code = code;
        trace->declBit(code, "__init_check__");
    }

    void init_check_dump_callback(void* user, VerilatedVcd::Buffer* buffer)
    {
        InitCheckTrace* state = static_cast<InitCheckTrace*>(user);
        buffer->fullBit(buffer->oldp(state->code), state->value);
    }
} // namespace

void trace_init()
{
    if (tfp)
    {
        init_check_trace.value = 0;
        tfp->spTrace()->addInitCb(init_check_init_callback, &init_check_trace, "", false, 1);
        // 首次记录也必须写检查位，否则全程通过时可能没有初值。
        tfp->spTrace()->addFullCb(init_check_dump_callback, 0, &init_check_trace);
        tfp->spTrace()->addChgCb(init_check_dump_callback, 0, &init_check_trace);
    }
}

void trace_step()
{
    if (tfp)
    {
        init_check_trace.value = init_check ? 1 : 0;
        tfp->dump(main_time);
    }
}

void reset(bool active)
{
    dut->MAIN_RESET_SIGNAL = active ? 1 : 0;
}

void clock_step()
{
    dut->MAIN_CLOCK_SIGNAL = !dut->MAIN_CLOCK_SIGNAL;
    dut->eval();
}

void advance_time()
{
    ++main_time;
    Verilated::threadContextp()->time(main_time);
}

// 只整理 VCD 的显示层级，不改变任何 DUT 信号或仿真时间。
bool hide_rootio_from_wave()
{
    std::ifstream input(MAIN_WAVE_PATH);
    const std::string temporary = std::string(MAIN_WAVE_PATH) + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!input || !output)
    {
        std::cerr << "SIM ERROR: cannot open waveform for scope cleanup\n";
        return false;
    }
    std::vector<std::string> header;
    std::vector<std::string> scopes;
    std::set<std::string> removed, visible;
    std::string line, check;
    bool definitions = false;
    while (std::getline(input, line))
    {
        std::stringstream stream(line);
        std::string keyword, type, name, code;
        stream >> keyword;
        if (keyword == "$scope")
        {
            stream >> type >> name;
            scopes.push_back(name);
            if (name != "$rootio")
            {
                header.push_back(line);
            }
        }
        else if (keyword == "$upscope")
        {
            if (!scopes.empty())
            {
                if (scopes.back() != "$rootio")
                {
                    header.push_back(line);
                }
                scopes.pop_back();
            }
        }
        else if (keyword == "$var")
        {
            stream >> type >> name >> code >> name;
            if (name == "__init_check__")
            {
                check = line;
            }
            else if (!scopes.empty() && scopes.back() == "$rootio")
            {
                removed.insert(code);
            }
            else
            {
                visible.insert(code);
                header.push_back(line);
            }
        }
        else if (keyword == "$enddefinitions")
        {
            definitions = true;
            break;
        }
        else
        {
            header.push_back(line);
        }
    }
    bool inserted = false;
    for (const std::string& record : header)
    {
        output << record << '\n';
        std::stringstream stream(record);
        std::string keyword, type, name;
        stream >> keyword >> type >> name;
        if (!inserted && keyword == "$scope" && type == "module" && name == MAIN_TEXT(MY_TOP))
        {
            output << check << '\n';
            inserted = true;
        }
    }
    if (!definitions || !inserted || check.empty())
    {
        std::cerr << "SIM ERROR: top/check scope missing in waveform; original waveform retained\n";
        output.close();
        std::remove(temporary.c_str());
        return false;
    }
    output << "$enddefinitions $end\n";
    while (std::getline(input, line))
    {
        const auto first = line.find_first_not_of(" \t");
        std::string code;
        if (first != std::string::npos)
        {
            const char kind = line[first];
            if (kind == 'b' || kind == 'r' || kind == 's')
            {
                std::stringstream stream(line.substr(first));
                std::string value;
                stream >> value >> code;
            }
            else if (kind == '0' || kind == '1' || kind == 'x' || kind == 'z')
            {
                code = line.substr(first + 1);
            }
        }
        if (!removed.count(code) || visible.count(code))
        {
            output << line << '\n';
        }
    }
    const bool read_ok = !input.bad();
    input.close();
    output.close();
    if (!read_ok || !output || std::rename(temporary.c_str(), MAIN_WAVE_PATH) != 0)
    {
        std::cerr << "SIM ERROR: waveform cleanup failed; original waveform retained\n";
        return false;
    }
    return true;
}
