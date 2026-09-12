#ifndef STIM_H
#define STIM_H

// STIM 实现依赖；stim.cpp 不再单独包含其他头文件。
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <vpi_user.h>

// STIM 专属配置，允许编译命令覆盖。
#ifndef STIM_FILE_PATH
#define STIM_FILE_PATH "constr/top.stim"
#endif
#ifndef STIM_HASH_COMMENT
#define STIM_HASH_COMMENT 1
#endif
#ifndef STIM_REPORT_PASS
#define STIM_REPORT_PASS 0
#endif
#ifndef STIM_REPORT_PATH
#define STIM_REPORT_PATH "stim.txt"
#endif
#ifndef STIM_MAX_WIDTH
#define STIM_MAX_WIDTH 65536
#endif

namespace stim
{
    using Time = uint64_t;
    // 未设置和显式设置为 0 是两种不同状态。
    struct Config
    {
        std::optional<Time> max_time;
        std::optional<Time> reset_time;
    };

    class Program
    {
    public:
        Program() = default;
        ~Program();
        Program(const Program&) = delete;
        Program& operator=(const Program&) = delete;

        // DUT 必须先创建。config_only 只读运行配置，不发现端口或执行事件。
        bool load(const std::string& path, const std::string& top, const std::string& scope,
                  const std::string& clock, const std::string& reset, bool config_only = false);
        const Config& config() const
        {
            return config_;
        }
        void start(Time release_time); // 初始化一次，忽略 <= release_time 的显式事件
        void apply(Time time);
        bool check(Time time); // 不匹配返回 false，不结束主仿真

    private:
        // 解析结果、真实端口信息，以及每次加载独立持有的运行状态。
        struct Diagnostic
        {
            int line;
            std::string level, message;
        };
        struct Assignment
        {
            std::string port, bits;
            int line = 0;
            bool expect = false;
        };
        struct Port
        {
            vpiHandle handle;
            int direction, width;
        };
        Config config_;
        std::string path_, clock_, reset_;
        bool ready_ = false;
        std::vector<Diagnostic> diagnostics_;
        std::map<std::string, Port> ports_;
        std::vector<Assignment> defaults_;
        std::map<Time, std::vector<Assignment>> events_;
        std::map<std::string, Assignment> expected_;
        std::ofstream report_;

        void clear();
        void diagnose(int line, const std::string& level, const std::string& message);
        bool print_diagnostics();
        void parse(const std::vector<std::string>& lines, const std::string& top, bool config_only);
        void parse_block(const std::string& text, int line);
        void bind(const std::string& scope);
        void validate(Assignment& value, bool is_default);
        void write(const Assignment& value);
    };
} // namespace stim
#endif
