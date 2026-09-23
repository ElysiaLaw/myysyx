module top(
	input clk,
	input rst,
    input [15:0]data,
    input serial_input,
    input [2:0]operate,
    output [15:0]outdata);

    shift_register #(16,1'b0) m1 (clk,rst,operate,serial_input,data,outdata);


endmodule
