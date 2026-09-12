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


module decoder #(INPUT_WID = 3, OUTPUT_WID = 8)(
    input enable,
    input [INPUT_WID -1:0] inputcode,
    output [OUTPUT_WID -1 :0] out);

    wire [OUTPUT_WID -1:0] [INPUT_WID -1:0] temp;
    
    genvar n;
    genvar m;
    generate
        for (m=0;m<OUTPUT_WID;m+=1) begin : decode_out
            for(n=0;n<INPUT_WID;n+=1) begin : decode_in
                assign temp[m][n] = inputcode[n] ^~ (((m >> n) & {{(OUTPUT_WID-1){1'B0}},1'b1} ) == 1);
            end
            assign out[m] = ( & temp[m] ) & enable;
        end
    endgenerate
endmodule
