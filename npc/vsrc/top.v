module top(
	input clk,
	input rst,
	input c,
	input d,
	output e);

	reg creg;

	always @(*)begin
		creg = c & d;
	end

	assign e =creg;

endmodule
