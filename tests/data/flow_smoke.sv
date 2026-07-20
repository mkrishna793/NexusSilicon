module flow_smoke (
    input  logic       clk,
    input  logic       rst_n,
    input  logic [7:0] a,
    input  logic [7:0] b,
    output logic [8:0] sum
);
    logic [8:0] acc;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            acc <= '0;
        else
            acc <= a + b;
    end

    assign sum = acc;
endmodule
