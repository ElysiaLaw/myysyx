module segout(
    input [3:0]data,
    input dot,
    output [7:0]segdata);

MuxKeyWithDefault #(10,4,7) m3 (.out(segdata[7:1]),.key(data),.default_out(7'b00000000),.lut({
    4'h0, ~7'b1111110, 
    4'h1, ~7'b0110000,
    4'h2, ~7'b1101101,
    4'h3, ~7'b1111001,
    4'h4, ~7'b0110011,
    4'h5, ~7'b1011011,
    4'h6, ~7'b1011111,
    4'h7, ~7'b1110000,
    4'h8, ~7'b1111111,
    4'h9, ~7'b1111011
    }));
    assign segdata[0] = dot;
    endmodule
