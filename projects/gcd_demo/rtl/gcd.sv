module gcd (
  input  logic       clk,
  input  logic       rst_n,
  input  logic       start,
  input  logic [7:0] a_in,
  input  logic [7:0] b_in,
  output logic [7:0] val_out,
  output logic       done
);

  logic [7:0] a, b;
  typedef enum logic [1:0] { IDLE, RUN, DONE } state_t;
  state_t state;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state   <= IDLE;
      val_out <= 8'h00;
      done    <= 1'b0;
    end else begin
      case (state)
        IDLE: begin
          done <= 1'b0;
          if (start) begin
            a     <= a_in;
            b     <= b_in;
            state <= RUN;
          end
        end
        RUN: begin
          if (a == b) begin
            val_out <= a;
            done    <= 1'b1;
            state   <= DONE;
          end else if (a > b) begin
            a <= a - b;
          end else begin
            b <= b - a;
          end
        end
        DONE: begin
          done  <= 1'b0;
          state <= IDLE;
        end
      endcase
    end
  end

endmodule
