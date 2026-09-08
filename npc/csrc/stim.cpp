#include "stim.h"

namespace stim
{
    std::string configured_top_name()
    {
        return STIM_TOP_NAME_TEXT(STIM_TOP_NAME);
    }

    namespace
    {
        Program current_program;
        std::map<std::string, vpiHandle> signal_handles;
        std::map<std::string, Value> previous_expectations;
        bool runtime_ready = false;
        bool expectation_mismatch = false;
        std::ofstream report_file;

        void report(const std::string& text)
        {
            if (report_file.is_open())
            {
                report_file << text << std::endl;
            }
        }

        void add_error(
            std::vector<Error>& errors,
            const std::string& file,
            int line,
            const std::string& message
        )
        {
            errors.push_back({file, line, message});
        }

        const Port* find_port(const std::string& name)
        {
            for (const Port& port : current_program.ports)
            {
                if (port.name == name)
                {
                    return &port;
                }
            }

            return nullptr;
        }

        bool parse_value(
            const std::string& text,
            Value& value,
            std::string& message
        )
        {
            // 兼容当前已有的单比特写法 a=0 / b=1。
            if (text == "0" || text == "1")
            {
                value.width = 1;
                value.bits = text;
                return true;
            }

            size_t base_position = text.find_first_of("bh");

            if (base_position == std::string::npos ||
                base_position == 0 ||
                base_position + 1 == text.size())
            {
                message = "invalid value";
                return false;
            }

            std::stringstream width_stream(
                text.substr(0, base_position)
            );

            int width;

            if (!(width_stream >> width) || width <= 0)
            {
                message = "invalid value width";
                return false;
            }

            char base = text[base_position];
            std::string digits =
                text.substr(base_position + 1);

            value.width = width;
            value.bits.clear();

            if (base == 'b')
            {
                if (static_cast<int>(digits.size()) != width)
                {
                    message = "binary value width mismatch";
                    return false;
                }

                for (char digit : digits)
                {
                    if (digit != '0' &&
                        digit != '1' &&
                        digit != 'x' &&
                        digit != 'X')
                    {
                        message = "invalid binary digit";
                        return false;
                    }

                    value.bits +=
                        digit == 'X' ? 'x' : digit;
                }

                return true;
            }

            if (base != 'h')
            {
                message = "value base must be b or h";
                return false;
            }

            int max_digits = (width + 3) / 4;

            if (static_cast<int>(digits.size()) > max_digits)
            {
                message = "hex value has too many digits";
                return false;
            }

            for (char digit : digits)
            {
                int number = -1;

                if (digit >= '0' && digit <= '9')
                {
                    number = digit - '0';
                }
                else if (digit >= 'a' && digit <= 'f')
                {
                    number = digit - 'a' + 10;
                }
                else if (digit >= 'A' && digit <= 'F')
                {
                    number = digit - 'A' + 10;
                }
                else if (digit == 'x' || digit == 'X')
                {
                    number = -2;
                }
                else
                {
                    message = "invalid hexadecimal digit";
                    return false;
                }

                for (int bit = 3; bit >= 0; bit--)
                {
                    if (number == -2)
                    {
                        value.bits += 'x';
                    }
                    else
                    {
                        value.bits +=
                            ((number >> bit) & 1) ? '1' : '0';
                    }
                }
            }

            int extra =
                static_cast<int>(value.bits.size()) - width;

            if (extra > 0)
            {
                for (int i = 0; i < extra; i++)
                {
                    if (value.bits[i] != '0' &&
                        value.bits[i] != 'x')
                    {
                        message = "hex value exceeds declared width";
                        return false;
                    }
                }

                value.bits = value.bits.substr(extra);
            }
            else if (extra < 0)
            {
                value.bits.insert(
                    value.bits.begin(),
                    -extra,
                    '0'
                );
            }

            return true;
        }

        bool validate_assignment(
            const Assignment& assignment,
            bool expectation,
            bool is_default,
            std::vector<Error>& errors,
            const std::string& file
        )
        {
            const Port* port = find_port(assignment.port);

            if (port == nullptr)
            {
                add_error(
                    errors,
                    file,
                    assignment.line,
                    "port does not exist: " + assignment.port
                );

                return false;
            }

            bool output_check = expectation;

            if (is_default)
            {
                output_check =
                    port->direction == PortDirection::Output;
            }

            if (output_check &&
                port->direction != PortDirection::Output)
            {
                add_error(
                    errors,
                    file,
                    assignment.line,
                    "expect requires an output port: " + assignment.port
                );

                return false;
            }

            if (!output_check &&
                port->direction != PortDirection::Input)
            {
                add_error(
                    errors,
                    file,
                    assignment.line,
                    "input assignment requires an input port: " + assignment.port
                );

                return false;
            }

            if (assignment.value.width != port->width)
            {
                add_error(
                    errors,
                    file,
                    assignment.line,
                    "value width does not match port width: " + assignment.port
                );

                return false;
            }

            if (!output_check)
            {
                for (char bit : assignment.value.bits)
                {
                    if (bit == 'x')
                    {
                        add_error(
                            errors,
                            file,
                            assignment.line,
                            "x is not allowed for input: " + assignment.port
                        );

                        return false;
                    }
                }
            }

            return true;
        }

        void put_value(
            vpiHandle handle,
            const Value& value
        )
        {
            int word_count = (value.width + 31) / 32;

            std::vector<s_vpi_vecval> words(
                word_count,
                {0, 0}
            );

            for (int i = 0; i < value.width; i++)
            {
                int position = value.width - i - 1;
                int word = position / 32;
                int bit = position % 32;

                if (value.bits[i] == '1')
                {
                    words[word].aval |= 1u << bit;
                }
                else if (value.bits[i] == 'x')
                {
                    words[word].bval |= 1u << bit;
                }
            }

            s_vpi_value vpi_value{};
            vpi_value.format = vpiVectorVal;
            vpi_value.value.vector = words.data();

            vpi_put_value(
                handle,
                &vpi_value,
                nullptr,
                vpiNoDelay
            );
        }

        bool match_value(
            vpiHandle handle,
            const Value& expected
        )
        {
            if (expected.width <= 32)
            {
                s_vpi_value vpi_value{};
                vpi_value.format = vpiIntVal;
                vpi_get_value(handle, &vpi_value);

                const uint32_t actual =
                    static_cast<uint32_t>(vpi_value.value.integer);

                for (int i = 0; i < expected.width; i++)
                {
                    if (expected.bits[i] == 'x')
                    {
                        continue;
                    }

                    const int position = expected.width - i - 1;
                    const bool actual_bit =
                        ((actual >> position) & 1u) != 0;
                    const bool expected_bit =
                        expected.bits[i] == '1';

                    if (actual_bit != expected_bit)
                    {
                        return false;
                    }
                }

                return true;
            }

            int word_count = (expected.width + 31) / 32;

            std::vector<s_vpi_vecval> words(
                word_count,
                {0, 0}
            );

            s_vpi_value vpi_value{};
            vpi_value.format = vpiVectorVal;
            vpi_value.value.vector = words.data();

            vpi_get_value(handle, &vpi_value);

            for (int i = 0; i < expected.width; i++)
            {
                if (expected.bits[i] == 'x')
                {
                    continue;
                }

                int position = expected.width - i - 1;
                int word = position / 32;
                int bit = position % 32;

                bool unknown =
                    (words[word].bval >> bit) & 1u;

                bool actual =
                    (words[word].aval >> bit) & 1u;

                bool wanted = expected.bits[i] == '1';

                if (unknown || actual != wanted)
                {
                    return false;
                }
            }

            return true;
        }

        void parse_port_field(
            const std::string& raw,
            const std::string& file,
            int line,
            std::vector<Port>& ports,
            std::vector<Error>& errors,
            bool& have_direction,
            PortDirection& last_direction,
            int& last_width
        )
        {
            std::string field = preprocess(raw);

            if (field.empty())
            {
                return;
            }

            std::stringstream stream(field);
            std::string first_word;
            stream >> first_word;

            PortDirection direction;
            bool explicit_direction = true;

            if (first_word == "input")
            {
                direction = PortDirection::Input;
            }
            else if (first_word == "output")
            {
                direction = PortDirection::Output;
            }
            else if (first_word == "inout")
            {
                add_error(
                    errors,
                    file,
                    line,
                    "inout port is not supported"
                );

                return;
            }
            else
            {
                if (!have_direction)
                {
                    add_error(
                        errors,
                        file,
                        line,
                        "port direction is missing"
                    );

                    return;
                }

                direction = last_direction;
                explicit_direction = false;
            }

            have_direction = true;

            std::string rest;
            std::getline(stream, rest);
            rest = preprocess(rest);

            if (!explicit_direction)
            {
                rest = first_word + " " + rest;
            }

            std::stringstream type_stream(rest);
            std::string type_word;
            type_stream >> type_word;

            if (type_word == "reg" ||
                type_word == "wire" ||
                type_word == "logic")
            {
                std::getline(type_stream, rest);
                rest = preprocess(rest);
            }

            int width = explicit_direction ? 1 : last_width;

            if (!rest.empty() && rest[0] == '[')
            {
                size_t right = rest.find(']');
                size_t colon = rest.find(':');

                if (right == std::string::npos ||
                    colon == std::string::npos)
                {
                    add_error(
                        errors,
                        file,
                        line,
                        "invalid port width"
                    );

                    return;
                }

                std::stringstream msb_stream(
                    preprocess(rest.substr(1, colon - 1))
                );

                std::stringstream lsb_stream(
                    preprocess(
                        rest.substr(
                            colon + 1,
                            right - colon - 1
                        )
                    )
                );

                int msb;
                int lsb;

                if (!(msb_stream >> msb) ||
                    !(lsb_stream >> lsb))
                {
                    add_error(
                        errors,
                        file,
                        line,
                        "invalid port width"
                    );

                    return;
                }

                width = msb >= lsb
                    ? msb - lsb + 1
                    : lsb - msb + 1;

                rest = preprocess(rest.substr(right + 1));
            }

            if (!rest.empty() && rest.back() == ';')
            {
                rest.pop_back();
                rest = preprocess(rest);
            }

            if (rest.empty())
            {
                add_error(
                    errors,
                    file,
                    line,
                    "port name is missing"
                );

                return;
            }

            for (const Port& old_port : ports)
            {
                if (old_port.name == rest)
                {
                    add_error(
                        errors,
                        file,
                        line,
                        "duplicate port: " + rest
                    );

                    return;
                }
            }

            ports.push_back({
                rest,
                direction,
                width,
                line,
                file
            });

            last_direction = direction;
            last_width = width;
        }
    }

    std::vector<std::string> readfile(const char* path)
    {
        std::vector<std::string> lines;
        std::ifstream fin(path);

        if (!fin.is_open())
        {
            report(
                "error file=" + std::string(path) +
                " line=0 message=failed to open"
            );
            return lines;
        }

        std::string line;

        while (std::getline(fin, line))
        {
            lines.push_back(line);
        }

        return lines;
    }

    std::string preprocess(const std::string& text)
    {
        std::string result = text;
        size_t comment_position = result.find("//");

#if STIM_HASH_COMMENT
        size_t hash_position = result.find('#');

        if (comment_position == std::string::npos ||
            (hash_position != std::string::npos &&
             hash_position < comment_position))
        {
            comment_position = hash_position;
        }
#endif

        if (comment_position != std::string::npos)
        {
            result = result.substr(0, comment_position);
        }

        size_t begin = 0;
        size_t end = result.size();

        while (begin < end &&
               std::isspace(
                   static_cast<unsigned char>(result[begin])))
        {
            begin++;
        }

        while (end > begin &&
               std::isspace(
                   static_cast<unsigned char>(result[end - 1])))
        {
            end--;
        }

        return result.substr(begin, end - begin);
    }

    std::vector<Port> get_port(
        std::vector<Error>& errors
    )
    {
        namespace fs = std::filesystem;
        std::vector<Port> ports;
        bool found_top = false;

        if (!fs::exists(STIM_V_PATH))
        {
            add_error(
                errors,
                STIM_V_PATH,
                0,
                "verilog directory does not exist"
            );

            return ports;
        }

        if (!fs::is_directory(STIM_V_PATH))
        {
            add_error(
                errors,
                STIM_V_PATH,
                0,
                "verilog path is not a directory"
            );

            return ports;
        }

        for (
            const fs::directory_entry& entry :
            fs::recursive_directory_iterator(STIM_V_PATH)
        )
        {
            if (!entry.is_regular_file() ||
                entry.path().extension() != ".v")
            {
                continue;
            }

            std::string file_path =
                entry.path().string();

            std::vector<std::string> lines =
                readfile(file_path.c_str());

            bool in_port_list = false;
            bool finished = false;
            bool have_direction = false;
            PortDirection last_direction =
                PortDirection::Input;
            int last_width = 1;

            std::string pending;
            int pending_line = 0;

            for (size_t i = 0; i < lines.size(); i++)
            {
                int line_number =
                    static_cast<int>(i + 1);

                std::string text =
                    preprocess(lines[i]);

                if (text.empty())
                {
                    continue;
                }

                std::string part = text;

                if (!in_port_list)
                {
                    std::stringstream module_stream(text);
                    std::string keyword;
                    std::string module_name;

                    module_stream >> keyword;
                    module_stream >> module_name;

                    size_t name_end =
                        module_name.find_first_of("(#");

                    if (name_end != std::string::npos)
                    {
                        module_name =
                            module_name.substr(0, name_end);
                    }

                    if (keyword != "module" ||
                        module_name != configured_top_name())
                    {
                        continue;
                    }

                    found_top = true;
                    in_port_list = true;

                    size_t left_parenthesis =
                        text.find('(');

                    if (left_parenthesis ==
                        std::string::npos)
                    {
                        add_error(
                            errors,
                            file_path,
                            line_number,
                            "module port list not found"
                        );

                        break;
                    }

                    part =
                        text.substr(left_parenthesis + 1);
                }

                size_t right_parenthesis =
                    part.find(')');

                if (right_parenthesis !=
                    std::string::npos)
                {
                    part =
                        part.substr(0, right_parenthesis);
                    finished = true;
                }

                size_t begin = 0;

                while (begin <= part.size())
                {
                    size_t comma =
                        part.find(',', begin);

                    std::string piece;

                    if (comma == std::string::npos)
                    {
                        piece = part.substr(begin);
                    }
                    else
                    {
                        piece =
                            part.substr(begin, comma - begin);
                    }

                    piece = preprocess(piece);

                    if (!piece.empty())
                    {
                        if (pending.empty())
                        {
                            pending_line = line_number;
                        }

                        if (!pending.empty())
                        {
                            pending += " ";
                        }

                        pending += piece;
                    }

                    if (comma == std::string::npos)
                    {
                        break;
                    }

                    parse_port_field(
                        pending,
                        file_path,
                        pending_line,
                        ports,
                        errors,
                        have_direction,
                        last_direction,
                        last_width
                    );

                    pending.clear();
                    pending_line = 0;
                    begin = comma + 1;
                }

                if (finished)
                {
                    break;
                }
            }

            if (!pending.empty())
            {
                parse_port_field(
                    pending,
                    file_path,
                    pending_line,
                    ports,
                    errors,
                    have_direction,
                    last_direction,
                    last_width
                );
            }

            if (in_port_list && !finished)
            {
                add_error(
                    errors,
                    file_path,
                    1,
                    "missing ')' in module port list"
                );
            }

            if (found_top)
            {
                break;
            }
        }

        if (!found_top)
        {
            add_error(
                errors,
                STIM_V_PATH,
                0,
                "top module not found"
            );
        }

        return ports;
    }

    bool load_program(
        Program& new_program,
        std::vector<Error>& errors
    )
    {
        report_file.open(STIM_REPORT_PATH, std::ios::out | std::ios::trunc);
        report("STIM report: " STIM_FILE_PATH);

        current_program = Program{};
        previous_expectations.clear();
        expectation_mismatch = false;
        current_program.ports = get_port(errors);

        std::vector<std::string> lines =
            readfile(STIM_FILE_PATH);

        bool found_top_line = false;
        std::string statement;
        int statement_line = 0;

        for (size_t i = 0; i < lines.size(); i++)
        {
            int line_number =
                static_cast<int>(i + 1);

            std::string text =
                preprocess(lines[i]);

            if (text.empty())
            {
                continue;
            }

            if (!found_top_line)
            {
                if (text.rfind("top=", 0) != 0)
                {
                    add_error(
                        errors,
                        STIM_FILE_PATH,
                        line_number,
                        "first line must be top=..."
                    );
                }
                else if (preprocess(text.substr(4)) !=
                         configured_top_name())
                {
                    add_error(
                        errors,
                        STIM_FILE_PATH,
                        line_number,
                        "top name does not match Makefile MY_TOP"
                    );
                }

                found_top_line = true;
                continue;
            }

            if (statement.empty())
            {
                statement_line = line_number;
            }

            if (!statement.empty())
            {
                statement += " ";
            }

            statement += text;

            if (statement.find('}') == std::string::npos)
            {
                continue;
            }

            size_t left = statement.find('{');
            size_t right = statement.find('}', left);

            if (left == std::string::npos ||
                right == std::string::npos)
            {
                add_error(
                    errors,
                    STIM_FILE_PATH,
                    statement_line,
                    "invalid assignment block"
                );

                statement.clear();
                continue;
            }

            std::string prefix =
                preprocess(statement.substr(0, left));

            bool expectation =
                prefix.rfind("expect", 0) == 0;

            bool is_default =
                prefix.find("@default") != std::string::npos;

            uint64_t time = 0;

            if (!is_default)
            {
                size_t at = prefix.find('@');

                if (at == std::string::npos)
                {
                    add_error(
                        errors,
                        STIM_FILE_PATH,
                        statement_line,
                        "time marker is missing"
                    );

                    statement.clear();
                    continue;
                }

                std::stringstream time_stream(
                    prefix.substr(at + 1)
                );

                if (!(time_stream >> time))
                {
                    add_error(
                        errors,
                        STIM_FILE_PATH,
                        statement_line,
                        "invalid time marker"
                    );

                    statement.clear();
                    continue;
                }
            }

            std::string body =
                statement.substr(
                    left + 1,
                    right - left - 1
                );

            size_t begin = 0;

            while (begin <= body.size())
            {
                size_t semicolon =
                    body.find(';', begin);

                std::string assignment_text;

                if (semicolon == std::string::npos)
                {
                    assignment_text = body.substr(begin);
                }
                else
                {
                    assignment_text =
                        body.substr(
                            begin,
                            semicolon - begin
                        );
                }

                assignment_text =
                    preprocess(assignment_text);

                if (!assignment_text.empty())
                {
                    size_t equal =
                        assignment_text.find('=');

                    if (equal == std::string::npos)
                    {
                        add_error(
                            errors,
                            STIM_FILE_PATH,
                            statement_line,
                            "assignment is missing '='"
                        );
                    }
                    else
                    {
                        Assignment assignment;
                        assignment.port =
                            preprocess(
                                assignment_text.substr(0, equal)
                            );
                        assignment.line = statement_line;

                        std::string value_text =
                            preprocess(
                                assignment_text.substr(equal + 1)
                            );

                        std::string message;

                        if (!parse_value(
                                value_text,
                                assignment.value,
                                message))
                        {
                            add_error(
                                errors,
                                STIM_FILE_PATH,
                                statement_line,
                                message
                            );
                        }
                        else if (validate_assignment(
                                     assignment,
                                     expectation,
                                     is_default,
                                     errors,
                                     STIM_FILE_PATH
                                 ))
                        {
                            if (is_default)
                            {
                                current_program.defaults.push_back(
                                    assignment
                                );
                            }
                            else if (expectation)
                            {
                                current_program.expects[time].push_back(
                                    assignment
                                );
                            }
                            else
                            {
                                current_program.inputs[time].push_back(
                                    assignment
                                );
                            }
                        }
                    }
                }

                if (semicolon == std::string::npos)
                {
                    break;
                }

                begin = semicolon + 1;
            }

            statement.clear();
        }

        if (!statement.empty())
        {
            add_error(
                errors,
                STIM_FILE_PATH,
                statement_line,
                "assignment block is missing '}'"
            );
        }

        if (errors.empty())
        {
            for (const Assignment& assignment :
                 current_program.defaults)
            {
                const Port* port =
                    find_port(assignment.port);

                if (port != nullptr &&
                    port->direction == PortDirection::Output)
                {
                    previous_expectations[assignment.port] =
                        assignment.value;
                }
            }
        }

        new_program = current_program;
        return errors.empty();
    }

    bool init_runtime(
        std::vector<Error>& errors
    )
    {
        signal_handles.clear();

        for (const Port& port : current_program.ports)
        {
            // main 使用空实例名后，顶层端口位于 TOP 作用域。
            std::string name =
                std::string("TOP.") + port.name;

            std::vector<char> mutable_name(
                name.begin(),
                name.end()
            );

            mutable_name.push_back('\0');

            vpiHandle handle =
                vpi_handle_by_name(
                    reinterpret_cast<PLI_BYTE8*>(
                        mutable_name.data()
                    ),
                    nullptr
                );

            if (handle == nullptr)
            {
                add_error(
                    errors,
                    port.file,
                    port.line,
                    "cannot find signal: " + name
                );
            }
            else
            {
                signal_handles[port.name] = handle;
            }
        }

        runtime_ready = errors.empty();
        return runtime_ready;
    }

    void apply_inputs(uint64_t time)
    {
        if (!runtime_ready)
        {
            return;
        }

        for (const Port& port : current_program.ports)
        {
            if (port.direction != PortDirection::Input)
            {
                continue;
            }

            const Assignment* selected = nullptr;

            auto event = current_program.inputs.find(time);

            if (event != current_program.inputs.end())
            {
                for (const Assignment& assignment : event->second)
                {
                    if (assignment.port == port.name)
                    {
                        selected = &assignment;
                    }
                }
            }

            if (selected == nullptr)
            {
                for (const Assignment& assignment :
                     current_program.defaults)
                {
                    if (assignment.port == port.name)
                    {
                        selected = &assignment;
                    }
                }
            }

            Value zero{
                port.width,
                std::string(port.width, '0')
            };

            if (selected == nullptr && time != 0)
            {
                continue;
            }

            put_value(
                signal_handles[port.name],
                selected == nullptr ? zero : selected->value
            );
        }
    }

    bool check_outputs(uint64_t time)
    {
        if (!runtime_ready)
        {
            return false;
        }

        auto event = current_program.expects.find(time);

        if (event != current_program.expects.end())
        {
            for (const Assignment& assignment : event->second)
            {
                previous_expectations[assignment.port] =
                    assignment.value;
            }
        }

        bool success = true;
        bool checked = false;

        for (const Port& port : current_program.ports)
        {
            if (port.direction != PortDirection::Output)
            {
                continue;
            }

            auto expected =
                previous_expectations.find(port.name);

            if (expected == previous_expectations.end())
            {
                continue;
            }

            checked = true;

            if (!match_value(
                    signal_handles[port.name],
                    expected->second
                ))
            {
                report(
                    "time=" + std::to_string(time) +
                    " port=" + port.name +
                    " expected=" + expected->second.bits +
                    " result=FAIL"
                );

                expectation_mismatch = true;
                success = false;
            }
            else
            {
#if STIM_REPORT_PASS
                report(
                    "time=" + std::to_string(time) +
                    " port=" + port.name +
                    " expected=" + expected->second.bits +
                    " result=PASS"
                );
#endif
            }
        }

#if STIM_REPORT_PASS
        if (!checked)
        {
            report(
                "time=" + std::to_string(time) +
                " result=SKIP"
            );
        }
#endif

        return success;
    }

    void finish_report()
    {
        if (expectation_mismatch)
        {
            std::cout << std::endl
                      << "存在不匹配的预期输入"
                      << std::endl;
        }
    }

    void print_errors(
        const std::vector<Error>& errors
    )
    {
        for (const Error& error : errors)
        {
            report(
                "error file=" + error.file +
                " line=" + std::to_string(error.line) +
                " message=" + error.message
            );
        }
    }
}
