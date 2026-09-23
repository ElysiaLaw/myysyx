module top(
	input clk,
	input rst,
	input [15:0]dataA,
    input [15:0]dataB,
    input [2:0]operate,
    output [15:0]outdata);

    ALU #(16) m1 (dataA,dataB,operate,outdata); 

endmodule
