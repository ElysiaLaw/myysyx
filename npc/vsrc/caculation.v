
// 0-add, 1-subtract, 2-not, 3-and, 4-or, 5-xor, 6-compare( A<B=>1 ; A>=B=>0 ),
// 7-equal ( A=B=>1 ; A!=B=>0 )
// the operate is 3 bit width
// the topest bit is the sign bit
module ALU #(WIDTH = 8)(
    input [WIDTH-1:0] dataA,
    input [WIDTH-1:0] dataB,
    input [2:0] operate,
    output[WIDTH-1:0] outdata);

    wire [WIDTH-1:0] result_add_sub;
    wire [WIDTH-1:0] result_not;
    wire [WIDTH-1:0] result_and;
    wire [WIDTH-1:0] result_or;
    wire [WIDTH-1:0] result_xor;
    wire result_compare;
    wire result_equal;
    wire subchoice;

    assign subchoice = &(operate ^~ 3'b001);

    /* verilator lint_off PINCONNECTEMPTY */

    add #(WIDTH) ALU_add(
         .a(dataA),
         .b( { WIDTH{subchoice}} ^ dataB ),
         .cin(subchoice),
         .out(result_add_sub),
         .cout());

    /* verilator lint_on PINCONNECTEMPTY */

    assign result_not = ~dataA;
    assign result_and = dataA & dataB;
    assign result_or = dataA |dataB;
    assign result_xor = dataA ^ dataB;
    
    compare_signed #(WIDTH) ALU_compare (.dataA(dataA),.dataB(dataB),.out(result_compare));
    
    assign result_equal = &(dataA ~^ dataB);
    
    MuxKey #(8,3,WIDTH) ALU_data_choice(
        .out(outdata),
        .key(operate),
        .lut({
        3'b000,result_add_sub,
        3'b001,result_add_sub,
        3'b010,result_not,
        3'b011,result_and,
        3'b100,result_or,
        3'b101,result_xor,
        3'b110,{ { (WIDTH-1){1'b0} } ,result_compare },
        3'b111,{ { (WIDTH-1){1'b0} } ,result_equal }  }));

endmodule


