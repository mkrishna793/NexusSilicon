# SDC Constraints file for GCD design
create_clock -name clk -period 1.0 [get_ports clk]
set_input_delay 0.2 -clock clk [all_inputs]
set_output_delay 0.2 -clock clk [all_outputs]
