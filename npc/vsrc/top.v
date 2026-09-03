module top(
	input clk,
	input rst,
	input a,
	input b,
	output c);

	reg creg;

	always @(*)begin
		creg = a & b;
	end

	assign c =creg;

endmodule
