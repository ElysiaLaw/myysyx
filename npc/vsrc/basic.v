module encoder #(INPUT_NUMBER = 8,OUT_LEN = 3)(
	output [OUT_LEN-1 : 0] out,
    output valid,
	input [INPUT_NUMBER-1 : 0] input_data);
	
	wire [INPUT_NUMBER -1:0] high_position;
	wire [INPUT_NUMBER :1] extra_input;
	wire [INPUT_NUMBER-1:0] valid_input;
    wire [OUT_LEN-1:0][INPUT_NUMBER-1:0] temp;

	assign extra_input[INPUT_NUMBER:1] = {1'b0,input_data[INPUT_NUMBER-1:1]};
    assign valid = | input_data;

	genvar n;
	generate
		for (n = 0; n <INPUT_NUMBER; n=n+1) begin: valid_preprocess
			assign high_position[n] = ~(| extra_input[INPUT_NUMBER:n+1]);
			assign valid_input[n] = high_position[n] & input_data[n];
		end
	endgenerate

	genvar m;
    generate 
        for (m =0; m < OUT_LEN;m=m+1) begin: encode_outside
            for (n=0;n<INPUT_NUMBER;n=n+1) begin: encode_inside
                assign temp[m][n] = valid_input[n] & ((( n >> m) & {{(OUT_LEN-1){1'b0}},1'b1}) == 1);
            end
            assign out[m] = |temp[m];
        end
    endgenerate
endmodule


module decoder #(INPUT_LEN = 3, OUTPUT_NUM = 8)(
    input enable,
    input [INPUT_LEN -1:0] inputcode,
    output [OUTPUT_NUM -1 :0] out);

    wire [OUTPUT_NUM -1:0] [INPUT_LEN -1:0] temp;
    
    genvar n;
    genvar m;
    generate
        for (m=0;m<OUTPUT_NUM;m+=1) begin : decode_out
            for(n=0;n<INPUT_LEN;n+=1) begin : decode_in
                assign temp[m][n] = inputcode[n] ^~ (((m >> n) & {{(OUTPUT_NUM-1){1'B0}},1'b1} ) == 1);
            end
            assign out[m] = ( & temp[m] ) & enable;
        end
    endgenerate
endmodule

module add #(ADD_LINE = 1)(
    input [ADD_LINE-1:0] a,
    input [ADD_LINE-1:0] b,
    input cin,
    output [ADD_LINE-1:0] out,
    output cout);

    wire [ADD_LINE:0] temp;
    assign temp[0] = cin;
    assign cout = temp[ADD_LINE];

    genvar n;
    generate
        for (n=0;n<ADD_LINE;n+=1)begin: add_init
            addInit init(.a(a[n]),.b(b[n]),.cin(temp[n]),.out(out[n]),.cout(temp[n+1]));
        end
    endgenerate
endmodule

module addInit (
    input a,
    input b,
    input cin,
    output out,
    output cout);
    
    wire e,f,g;

    assign e = a ^ b;
    assign out = cin ^ e;
    assign f = cin & e;
    assign g = a & b;
    assign cout = f|g;
 endmodule

/* verilator lint_off UNUSEDSIGNAL */

//( A<B => 1 else is 0)
module compare_unsigned #(WIDTH = 8)(
    input [WIDTH-1:0] dataA,
    input [WIDTH-1:0] dataB,
    output out);
    wire [WIDTH-1:0] outtemp;
    wire [WIDTH : 0] downcheck;
    wire [WIDTH-1:0] outdata;

    assign downcheck[WIDTH] = 1'b1;

    genvar n;
    generate
        for(n=0;n<WIDTH;n+=1)begin : compare_unsigned_init
            assign outtemp[n] = (~dataA[n]) & dataB[n];
            assign downcheck[n] = dataA[n] ^~ dataB[n];
            assign outdata[n] = outtemp[n] &( &downcheck[WIDTH:n+1] );
        end
    endgenerate

    assign out = | outdata[WIDTH-1:0];

endmodule

/* verilator lint_on UNUSEDSIGNAL */


// A<B=>1 else is 0
module compare_signed #(WIDTH = 8) (
    input [WIDTH-1:0] dataA,
    input [WIDTH-1:0] dataB,
    output out);
    wire [1:0] outdata;

    assign outdata[1] = dataA[WIDTH-1] &(~dataB[WIDTH-1]);
    compare_unsigned #(WIDTH-1) compare_signed_init(.dataA(dataA[WIDTH-2:0]), .dataB(dataB[WIDTH-2:0]), .out( outdata[0]));
    assign out =outdata[1] | ((dataA[WIDTH-1] ~^ dataB[WIDTH-1] ) & outdata[0]);

endmodule

//operate width is 3 bit ; which
//0-reset; 1-set; 2-logical_right ; 3-logical_left; 4-arithmetic_right; 
//5-(logical_right then input 1 bit from left and output all of the bit)
//6-right_roration ; 7-left_roration
module shift_register #(DATA_LEN = 8,DEFAULT_NUM=1'b0)( //这个DEFAULT的定义应当为一bit位宽，用于默认填充
    input clk,
    input rst,
    input [2:0] operate,
    input serial_input,
    input [DATA_LEN-1:0] parallel_input,
    output [DATA_LEN-1:0] outdata);

    wire [DATA_LEN-1:0]reg_input;

    Reg #(DATA_LEN,0) shiftreg_reg (
        .clk(clk),
        .rst( ( ~( |operate ) ) | rst ),
        .din(reg_input),
        .dout(outdata),
        .wen(1'b1));

    MuxKey #(7,3,DATA_LEN) shift_mux (
        .out(reg_input),
        .key(operate),
        .lut({
        3'b001,parallel_input,
        3'b010,{DEFAULT_NUM,outdata[DATA_LEN-1:1]},
        3'b011,{outdata[DATA_LEN-2:0],DEFAULT_NUM},
        3'b100,{outdata[DATA_LEN-1],outdata[DATA_LEN-1:1]},
        3'b101,{serial_input,outdata[DATA_LEN-1:1]},
        3'b110,{outdata[0],outdata[DATA_LEN-1:1]},
        3'b111,{outdata[DATA_LEN-2:0],outdata[DATA_LEN-1]}
        }));

endmodule
