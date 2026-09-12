#include "stim.h"

namespace stim
{
    // 文本预处理与数值校验，只服务于 STIM 语法。
    namespace
    {
        std::string trim(const std::string& text)
        {
            const auto first = text.find_first_not_of(" \t\r\n\f\v");
            if (first == std::string::npos)
            {
                return {};
            }
            return text.substr(first, text.find_last_not_of(" \t\r\n\f\v") - first + 1);
        }

        std::string preprocess(const std::string& text)
        {
            size_t end = text.find("//");
#if STIM_HASH_COMMENT
            end = std::min(end, text.find('#'));
#endif
            return trim(text.substr(0, end));
        }

        bool number(const std::string& text, Time& value)
        {
            if (text.empty())
            {
                return false;
            }
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
            return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
                   value < std::numeric_limits<Time>::max();
        }

        bool identifier(const std::string& name)
        {
            if (name.empty() ||
                !(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_'))
            {
                return false;
            }
            for (unsigned char c : name)
            {
                if (!std::isalnum(c) && c != '_' && c != '$')
                {
                    return false;
                }
            }
            return true;
        }

        bool value_bits(const std::string& text, std::string& bits, std::string& error)
        {
            if (text == "0" || text == "1")
            {
                bits = text;
                return true;
            }
            const size_t base = text.find_first_of("bh");
            Time width = 0;
            if (base == std::string::npos || !number(text.substr(0, base), width) || width == 0 ||
                width > STIM_MAX_WIDTH)
            {
                error = "invalid width/value; use e.g. 1b0, 8b00000001 or 8h01";
                return false;
            }
            const std::string digits = text.substr(base + 1);
            if (digits.empty() || (text[base] == 'b' && digits.size() != width) ||
                (text[base] == 'h' && digits.size() > (width + 3) / 4))
            {
                error = "digit count does not match declared width";
                return false;
            }
            bits.clear();
            for (char c : digits)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (text[base] == 'b')
                {
                    if (c != '0' && c != '1' && c != 'x')
                    {
                        error = "binary digits must be 0, 1 or x";
                        return false;
                    }
                    bits += c;
                }
                else
                {
                    if (c == 'x')
                    {
                        bits += "xxxx";
                        continue;
                    }
                    int digit = c >= '0' && c <= '9'   ? c - '0'
                                : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                                       : -1;
                    if (digit < 0)
                    {
                        error = "invalid hexadecimal digit";
                        return false;
                    }
                    for (int bit = 3; bit >= 0; --bit)
                    {
                        bits += ((digit >> bit) & 1) ? '1' : '0';
                    }
                }
            }
            if (bits.size() > width)
            {
                const size_t extra = bits.size() - width;
                if (bits.substr(0, extra).find('1') != std::string::npos)
                {
                    error = "value exceeds declared width";
                    return false;
                }
                bits.erase(0, extra);
            }
            if (bits.size() < width)
            {
                bits.insert(0, width - bits.size(), '0');
            }
            return true;
        }
    } // namespace

    // 加载生命周期：先清理旧状态，诊断完成后才允许驱动。
    Program::~Program()
    {
        clear();
    }

    void Program::clear()
    {
        ready_ = false;
        for (const auto& item : ports_)
        {
            vpi_release_handle(item.second.handle);
        }
        ports_.clear();
        defaults_.clear();
        events_.clear();
        expected_.clear();
        diagnostics_.clear();
        config_ = {};
        if (report_.is_open())
        {
            report_.close();
        }
        report_.clear();
    }

    void Program::diagnose(int line, const std::string& level, const std::string& message)
    {
        diagnostics_.push_back({line, level, message});
    }

    bool Program::print_diagnostics()
    {
        bool success = true;
        std::stable_sort(diagnostics_.begin(), diagnostics_.end(),
                         [](const Diagnostic& a, const Diagnostic& b) { return a.line < b.line; });
        if (!diagnostics_.empty())
        {
            std::cerr << '\n';
        }
        for (const auto& d : diagnostics_)
        {
            std::cerr << "STIM " << d.level << " " << path_;
            if (d.line > 0)
            {
                std::cerr << ':' << d.line;
            }
            std::cerr << ": " << d.message << '\n';
            if (d.level == "ERROR")
            {
                success = false;
            }
        }
        return success;
    }

    bool Program::load(const std::string& path, const std::string& top, const std::string& scope,
                       const std::string& clock, const std::string& reset, bool config_only)
    {
        clear();
        path_ = path;
        clock_ = clock;
        reset_ = reset;
        std::ifstream input(path);
        if (!input)
        {
            diagnose(0, config_only ? "WARNING" : "ERROR",
                     "cannot open script; runtime defaults retained");
            print_diagnostics();
            return false;
        }
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(input, line))
        {
            lines.push_back(preprocess(line));
        }
        if (input.bad())
        {
            diagnose(0, config_only ? "WARNING" : "ERROR", "failed while reading script");
        }
        parse(lines, top, config_only);
        if (!config_only)
        {
            bind(scope);
        }
        const bool success = print_diagnostics();
        ready_ = success && !config_only;
        return success;
    }

    // 先按语句收集配置与事件，再统一检查真实端口。
    void Program::parse(const std::vector<std::string>& lines, const std::string& top,
                        bool config_only)
    {
        bool header = false, skip_block = false;
        std::string pending;
        int first_line = 0;
        for (size_t i = 0; i < lines.size(); ++i)
        {
            const std::string& text = lines[i];
            const int line = static_cast<int>(i + 1);
            if (text.empty())
            {
                if (!pending.empty())
                {
                    pending += '\n';
                }
                continue;
            }
            if (!header)
            {
                header = true;
                const auto equal = text.find('=');
                if (equal == std::string::npos || trim(text.substr(0, equal)) != "top" ||
                    trim(text.substr(equal + 1)) != top)
                {
                    diagnose(line, config_only ? "WARNING" : "ERROR",
                             "expected first statement top=" + top + ", got: " + text);
                    if (config_only)
                    {
                        return;
                    }
                }
                continue;
            }
            const bool event_start = text[0] == '@' || text.rfind("expect", 0) == 0;
            if (config_only)
            {
                if (skip_block)
                {
                    if (text.find('}') != std::string::npos)
                    {
                        skip_block = false;
                    }
                    continue;
                }
                if (event_start || text.find('{') != std::string::npos)
                {
                    skip_block = text.find('}') == std::string::npos;
                    continue;
                }
            }
            if (!pending.empty() && event_start)
            {
                diagnose(first_line, "ERROR", "missing '}' before the next statement");
                pending.clear();
            }
            if (pending.empty())
            {
                const auto equal = text.find('=');
                if (!event_start && text.find('{') == std::string::npos)
                {
                    const std::string key = trim(text.substr(0, equal));
                    if ((key == "max_time" || key == "reset_time") && equal != std::string::npos)
                    {
                        auto& setting = key == "max_time" ? config_.max_time : config_.reset_time;
                        Time parsed = 0;
                        const std::string raw = trim(text.substr(equal + 1));
                        if (!number(raw, parsed))
                        {
                            diagnose(line, "WARNING",
                                     "invalid " + key + "='" + raw +
                                         "'; expected unsigned decimal; " +
                                         (setting ? "retaining previous valid value " +
                                                        std::to_string(*setting)
                                                  : "using main/Makefile default"));
                        }
                        else
                        {
                            if (setting)
                            {
                                diagnose(line, "WARNING",
                                         "repeated " + key + "; last valid value wins: " + raw);
                            }
                            setting = parsed;
                        }
                    }
                    else
                    {
                        diagnose(line, config_only ? "WARNING" : "ERROR",
                                 "unknown/malformed statement: " + text +
                                     "; use max_time=..., reset_time=... or @time {...}");
                    }
                    continue;
                }
                first_line = line;
            }
            if (config_only)
            {
                continue;
            }
            if (!pending.empty())
            {
                pending += '\n';
            }
            pending += text;
            if (text.find('}') != std::string::npos)
            {
                parse_block(pending, first_line);
                pending.clear();
            }
        }
        if (!header)
        {
            diagnose(0, config_only ? "WARNING" : "ERROR", "empty script; expected top=" + top);
        }
        if (!pending.empty())
        {
            diagnose(first_line, "ERROR", "assignment block is missing '}'");
        }
    }

    void Program::parse_block(const std::string& text, int line)
    {
        const auto left = text.find('{'), right = text.find('}');
        if (left == std::string::npos || right < left)
        {
            diagnose(line, "ERROR", "invalid assignment block");
            return;
        }
        if (!trim(text.substr(right + 1)).empty())
        {
            diagnose(line, "ERROR", "unexpected text after '}'; one block per line");
        }
        if (text.find('{', left + 1) < right)
        {
            diagnose(line, "ERROR", "nested assignment blocks are not supported");
            return;
        }
        std::string prefix = trim(text.substr(0, left));
        bool expect = false;
        if (prefix.rfind("expect", 0) == 0 && prefix.size() > 6 &&
            std::isspace(static_cast<unsigned char>(prefix[6])))
        {
            expect = true;
            prefix = trim(prefix.substr(6));
        }
        const bool is_default = prefix == "@default" && !expect;
        Time time = 0;
        if (!is_default && (prefix.empty() || prefix[0] != '@' || !number(prefix.substr(1), time)))
        {
            diagnose(line, "ERROR",
                     "expected @default, @<unsigned time> or expect @<unsigned time>");
            return;
        }
        const std::string body = text.substr(left + 1, right - left - 1);
        size_t begin = 0;
        while (begin < body.size())
        {
            const auto end = body.find(';', begin);
            const std::string raw =
                body.substr(begin, end == std::string::npos ? end : end - begin);
            const auto first = raw.find_first_not_of(" \t\r\n");
            if (first != std::string::npos)
            {
                const size_t offset = left + 1 + begin + first;
                const int source_line =
                    line + static_cast<int>(std::count(text.begin(), text.begin() + offset, '\n'));
                const std::string item = trim(raw);
                const auto equal = item.find('=');
                Assignment a;
                a.port = trim(item.substr(0, equal));
                a.line = source_line;
                a.expect = expect;
                std::string error;
                if (equal == std::string::npos || !identifier(a.port))
                {
                    diagnose(source_line, "ERROR", "expected port=value, got: " + item);
                }
                else if (!value_bits(trim(item.substr(equal + 1)), a.bits, error))
                {
                    diagnose(source_line, "ERROR", a.port + ": " + error);
                }
                else if (is_default)
                {
                    defaults_.push_back(a);
                }
                else
                {
                    events_[time].push_back(a);
                }
            }
            if (end == std::string::npos)
            {
                break;
            }
            begin = end + 1;
        }
    }

    // 端口信息来自已经构建的模型，不扫描或猜测 Verilog 声明。
    void Program::bind(const std::string& scope)
    {
        vpiHandle root = vpi_handle_by_name(const_cast<char*>(scope.c_str()), nullptr);
        if (!root)
        {
            diagnose(0, "ERROR", "VPI scope not found: " + scope);
            return;
        }
        vpiHandle iterator = vpi_iterate(vpiReg, root);
        vpi_release_handle(root);
        if (!iterator)
        {
            diagnose(0, "ERROR", "no VPI variables exposed in " + scope);
            return;
        }
        while (vpiHandle handle = vpi_scan(iterator))
        {
            const int direction = vpi_get(vpiDirection, handle);
            const int width = vpi_get(vpiSize, handle);
            const std::string name = vpi_get_str(vpiName, handle);
            if (direction != vpiInput && direction != vpiOutput && direction != vpiInout)
            {
                vpi_release_handle(handle);
                continue;
            }
            if (direction == vpiInout || vpi_get(vpiType, handle) == vpiRegArray || width <= 0 ||
                width > STIM_MAX_WIDTH)
            {
                diagnose(0, "ERROR", "unsupported port (inout/unpacked array/width): " + name);
                vpi_release_handle(handle);
                continue;
            }
            ports_.emplace(name, Port{handle, direction, width});
        }
        if (ports_.empty())
        {
            diagnose(0, "ERROR", "no input/output ports found in " + scope);
        }
        for (auto& value : defaults_)
        {
            validate(value, true);
        }
        for (auto& event : events_)
        {
            for (auto& value : event.second)
            {
                validate(value, false);
            }
        }
    }

    void Program::validate(Assignment& value, bool is_default)
    {
        const auto found = ports_.find(value.port);
        if (found == ports_.end())
        {
            diagnose(value.line, "ERROR", "port does not exist: " + value.port);
            return;
        }
        const Port& port = found->second;
        if (is_default)
        {
            value.expect = port.direction == vpiOutput;
        }
        if (value.port == clock_ || (is_default && value.port == reset_))
        {
            diagnose(value.line, "ERROR",
                     "main owns clock/startup reset; use explicit post-reset rst events only: " +
                         value.port);
        }
        if ((value.expect && port.direction != vpiOutput) ||
            (!value.expect && port.direction != vpiInput))
        {
            diagnose(value.line, "ERROR", "wrong port direction for assignment: " + value.port);
        }
        if (value.bits.size() != static_cast<size_t>(port.width))
        {
            diagnose(value.line, "ERROR",
                     value.port + ": port width=" + std::to_string(port.width) +
                         ", value width=" + std::to_string(value.bits.size()));
        }
        if (!value.expect && value.bits.find('x') != std::string::npos)
        {
            diagnose(value.line, "ERROR",
                     "x is only allowed in output expectations: " + value.port);
        }
    }

    void Program::write(const Assignment& value)
    {
        s_vpi_value data{};
        data.format = vpiBinStrVal;
        data.value.str = const_cast<char*>(value.bits.c_str());
        vpi_put_value(ports_.at(value.port).handle, &data, nullptr, vpiNoDelay);
    }

    // start 只初始化一次；apply 只更新当前刻度存在的事件。
    void Program::start(Time release_time)
    {
        if (!ready_)
        {
            return;
        }
        size_t ignored = 0;
        auto next = events_.begin();
        while (next != events_.end() && next->first <= release_time)
        {
            ignored += next->second.size();
            next = events_.erase(next);
        }
        if (ignored)
        {
            std::cerr << "STIM WARNING " << path_ << ": ignored " << ignored
                      << " assignments at/before reset release t=" << release_time << '\n';
        }
        for (const auto& item : ports_)
        {
            if (item.second.direction == vpiInput && item.first != clock_ && item.first != reset_)
            {
                write({item.first, std::string(item.second.width, '0'), 0, false});
            }
        }
        for (const auto& value : defaults_)
        {
            if (value.expect)
            {
                expected_[value.port] = value;
            }
            else
            {
                write(value);
            }
        }
#if STIM_REPORT_PASS
        report_.open(STIM_REPORT_PATH, std::ios::trunc);
        if (!report_)
        {
            std::cerr << "STIM WARNING " << STIM_REPORT_PATH << ": cannot save PASS/SKIP report\n";
        }
#endif
    }

    void Program::apply(Time time)
    {
        if (!ready_)
        {
            return;
        }
        const auto found = events_.find(time);
        if (found == events_.end())
        {
            return;
        }
        for (const auto& value : found->second)
        {
            if (value.expect)
            {
                expected_[value.port] = value;
            }
            else
            {
                write(value);
            }
        }
    }

    // x 是期望掩码；这里只返回匹配结果，不控制主仿真退出。
    bool Program::check(Time time)
    {
        if (!ready_)
        {
            return true;
        }
        bool success = true;
        for (const auto& item : expected_)
        {
            const auto& expected = item.second;
            s_vpi_value data{};
            data.format = vpiBinStrVal;
            vpi_get_value(ports_.at(item.first).handle, &data);
            const std::string actual = data.value.str ? data.value.str : "";
            bool match = actual.size() == expected.bits.size();
            for (size_t i = 0; match && i < actual.size(); ++i)
            {
                if (expected.bits[i] != 'x' && expected.bits[i] != actual[i])
                {
                    match = false;
                }
            }
            if (!match)
            {
                success = false;
                std::cerr << "STIM ERROR " << path_ << ':' << expected.line << ": time=" << time
                          << " port=" << item.first << " expected=" << expected.bits
                          << " actual=" << actual << " result=FAIL\n";
            }
#if STIM_REPORT_PASS
            else if (report_)
            {
                report_ << "time=" << time << " port=" << item.first << " result=PASS\n";
            }
#endif
        }
#if STIM_REPORT_PASS
        if (expected_.empty() && report_)
        {
            report_ << "time=" << time << " result=SKIP\n";
        }
#endif
        return success;
    }
} // namespace stim
