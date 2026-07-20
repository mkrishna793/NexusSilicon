# Invoked by an OpenROAD binary compiled with the OpenRCX module. MFE owns routing;
# this adapter only loads MFE-exported LEF/DEF geometry to produce SPEF.
if {$argc != 5} {
  puts stderr "usage: run-openrcx.tcl <tech.lef> <cells.lef> <routed.def> <rules.rcx> <output.spef>"
  exit 2
}
set tech_lef [lindex $argv 0]
set cells_lef [lindex $argv 1]
set routed_def [lindex $argv 2]
set rcx_rules [lindex $argv 3]
set output_spef [lindex $argv 4]
read_lef $tech_lef
read_lef $cells_lef
read_def $routed_def
extract_parasitics -ext_model_file $rcx_rules
write_spef $output_spef
