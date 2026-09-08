#include "myuse.h"

namespace
{
	struct InitCheckTrace
	{
		uint32_t code = 0;
		CData value = 0;
	};

	InitCheckTrace init_check_trace;

	void init_check_init_callback(
		void* user,
		VerilatedVcd* trace,
		uint32_t code)
	{
		InitCheckTrace* state =
			static_cast<InitCheckTrace*>(user);
		state->code = code;
		trace->declBit(code, "__init_check__");
	}

	void init_check_dump_callback(
		void* user,
		VerilatedVcd::Buffer* buffer)
	{
		InitCheckTrace* state =
			static_cast<InitCheckTrace*>(user);
		buffer->fullBit(buffer->oldp(state->code), state->value);
	}
}

void trace_init()
{
#ifdef TRACE_ON
	init_check_trace.value = 0;
	tfp->spTrace()->addInitCb(
		init_check_init_callback,
		&init_check_trace,
		"",
		false,
		1);
	tfp->spTrace()->addChgCb(
		init_check_dump_callback,
		0,
		&init_check_trace);
#endif
}

void trace_step()
{
#ifdef TRACE_ON
	init_check_trace.value = init_check ? 1 : 0;
	tfp->dump(main_time++);
#endif
}

namespace myclock_time
{
	void single_cycle()
	{
		myclock_time::half_single_posedge();
		trace_step();
		myclock_time::half_single_negedge();
		trace_step();
	}


	void half_single_posedge()
	{
		dut -> clk = 1;
		dut -> eval();
	}
	
	void half_single_negedge()
	{
		dut -> clk = 0;
		dut -> eval();
	}

}


void reset(int n)
{
	while(n)
	{
		dut ->rst = 1;
		n -= 1;
		init_check = 0;
		trace_step();
	}
	dut -> rst = 0 ;
	init_check = 0;
	trace_step();
}

void hide_rootio_from_wave()
{
	std::ifstream input("wave.vcd");
	std::ofstream output("wave.vcd.tmp");
	std::set<std::string> rootio_codes;
	std::string line;
	std::string init_check_declaration;
	bool in_rootio = false;
	int rootio_depth = 0;
	int visible_scope_depth = 0;

	if (!input.is_open() || !output.is_open())
	{
		return;
	}

	while (std::getline(input, line))
	{
		if (line.find("$scope module $rootio $end") !=
			std::string::npos)
		{
			in_rootio = true;
			rootio_depth = 1;
			continue;
		}

		if (in_rootio)
		{
			if (line.find("$scope") != std::string::npos)
			{
				rootio_depth++;
				output << line << '\n';
				continue;
			}

			if (line.find("$upscope") != std::string::npos)
			{
				rootio_depth--;
				if (rootio_depth == 0)
				{
					in_rootio = false;
				}
				else
				{
					output << line << '\n';
				}
				continue;
			}

			if (rootio_depth == 1 &&
				line.find("$var") != std::string::npos)
			{
				std::stringstream stream(line);
				std::string keyword;
				std::string type;
				std::string width;
				std::string code;
				stream >> keyword >> type >> width >> code;
				rootio_codes.insert(code);
				continue;
			}

			if (rootio_depth > 1)
			{
				output << line << '\n';
			}
			continue;
		}

		if (line.find("$scope") != std::string::npos)
		{
			visible_scope_depth++;
			output << line << '\n';
			continue;
		}

		if (line.find("$upscope") != std::string::npos)
		{
			if (visible_scope_depth == 1 &&
				!init_check_declaration.empty())
			{
				output << init_check_declaration << '\n';
				init_check_declaration.clear();
			}

			output << line << '\n';
			if (visible_scope_depth > 0)
			{
				visible_scope_depth--;
			}
			continue;
		}

		if (line.find("$var") != std::string::npos &&
			line.find("__init_check__") != std::string::npos)
		{
			init_check_declaration = line;
			continue;
		}

		if (line == "$enddefinitions $end" &&
			!init_check_declaration.empty())
		{
			output << init_check_declaration << '\n';
			init_check_declaration.clear();
		}

		bool remove_value = false;
		const size_t first = line.find_first_not_of(" \t");
		if (first != std::string::npos &&
			line[first] != '#' && line[first] != '$')
		{
			if (line[first] == 'b')
			{
				const size_t space = line.find(' ', first);
				if (space != std::string::npos)
				{
					remove_value = rootio_codes.count(
						line.substr(space + 1)) != 0;
				}
			}
			else if (line.size() > first + 1)
			{
				remove_value = rootio_codes.count(
					line.substr(first + 1, 1)) != 0;
			}
		}

		if (!remove_value)
		{
			output << line << '\n';
		}
	}

	input.close();
	output.close();

	std::ifstream staged_input("wave.vcd.tmp");
	std::ofstream reordered_output("wave.vcd.tmp2");
	std::vector<std::string> staged_lines;
	std::string staged_line;
	std::string staged_init_check;

	while (std::getline(staged_input, staged_line))
	{
		if (staged_line.find("$var") != std::string::npos &&
			staged_line.find("__init_check__") != std::string::npos)
		{
			staged_init_check = staged_line;
			continue;
		}

		staged_lines.push_back(staged_line);
	}

	staged_input.close();

	bool top_scope_seen = false;
	bool init_check_inserted = false;

	for (const std::string& current_line : staged_lines)
	{
		if (current_line.find("$scope") != std::string::npos)
		{
			top_scope_seen = true;
		}

		if (top_scope_seen &&
			current_line.find("$upscope") != std::string::npos &&
			!init_check_inserted &&
			!staged_init_check.empty())
		{
			reordered_output << staged_init_check << '\n';
			init_check_inserted = true;
		}

		if (current_line == "$enddefinitions $end" &&
			!init_check_inserted &&
			!staged_init_check.empty())
		{
			reordered_output << staged_init_check << '\n';
			init_check_inserted = true;
		}

		reordered_output << current_line << '\n';
	}

	reordered_output.close();
	std::remove("wave.vcd.tmp");
	std::rename("wave.vcd.tmp2", "wave.vcd.tmp");
	std::remove("wave.vcd");
	std::rename("wave.vcd.tmp", "wave.vcd");
}
