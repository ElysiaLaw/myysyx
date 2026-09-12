module top(
	input clk,
	input rst,
	input [7:0]data,
    output valid,
	output [2:0]out,
    output [7:0]out2);

encoder #(8,3) m1 (.input_data(data),.out(out),.valid(valid));
decoder #(3,8) m2 (.enable(valid),.inputcode(out),.out(out2));

endmodule
