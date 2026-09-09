#ifndef __STIM_H__
#define __STIM_H__

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <vpi_user.h>

#ifndef STIM_REPORT_PASS
#define STIM_REPORT_PASS 0
#endif

#ifndef STIM_HASH_COMMENT
#define STIM_HASH_COMMENT 1
#endif

#define STIM_V_PATH "vsrc"
#define STIM_FILE_PATH "constr/top.stim"
#define STIM_REPORT_PATH "stim.txt"

#ifndef STIM_TOP_NAME
#error "STIM_TOP_NAME must be supplied by Makefile"
#endif

#define STIM_TOP_NAME_TEXT_2(name) #name
#define STIM_TOP_NAME_TEXT(name) STIM_TOP_NAME_TEXT_2(name)
namespace stim
{
    enum class PortDirection
    {
        Input,
        Output,
        Inout
    };

    struct Port
    {
        std::string name;
        PortDirection direction;
        int width;
        int line;
        std::string file;
    };

    struct Error
    {
        std::string file;
        int line;
        std::string message;
    };

    struct Value
    {
        int width;
        std::string bits;
    };

    struct Assignment
    {
        std::string port;
        Value value;
        int line;
    };

    struct Program
    {
        std::string top_name;
        std::vector<Port> ports;
        std::vector<Assignment> defaults;
        std::map<uint64_t, std::vector<Assignment>> inputs;
        std::map<uint64_t, std::vector<Assignment>> expects;
    };

    std::vector<std::string> readfile(const char* path);
    std::string preprocess(const std::string& text);
    std::vector<Port> get_port(std::vector<Error>& errors);
    bool load_program(Program& program, std::vector<Error>& errors);
    bool init_runtime(std::vector<Error>& errors);
    void apply_inputs(uint64_t time);
    bool check_outputs(uint64_t time);
    void finish_report();
    void print_errors(const std::vector<Error>& errors);
}

#endif
