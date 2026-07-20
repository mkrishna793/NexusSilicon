# External adapter contracts

These adapters are intentionally file/command based. They preserve MFE as the source of
truth and avoid sharing mutable internal databases with external projects.

## Synthesis

`run-yosys.ps1` accepts SystemVerilog/Verilog, a top module, and optionally a Liberty
library. It emits a mapped gate-level Verilog netlist plus a hash-bearing manifest. A
physical DEF is produced later by MFE after floorplanning and placement.

## Timing

`run-opensta.ps1` consumes mapped Verilog, Liberty, SDC, and optional SPEF. It emits the
raw timing report and a manifest. An MFE parser will convert reported paths into evidence
graph entities and findings.

## Extraction

`run-openrcx.ps1` invokes `run-openrcx.tcl` through an OpenROAD/OpenDB host. It consumes MFE-exported routed DEF and PDK LEF/RC rule inputs through an
OpenROAD/OpenDB host, then writes SPEF for OpenSTA. No OpenROAD placement or routing command
is used. It also writes an extraction manifest consumed by the evidence-graph importer.

## Required PDK inputs

A physical run requires technology LEF, cell/macro LEF, at least one Liberty timing corner,
and RC extraction rules. The current ASAP7 technology LEF alone is not enough because it
does not include standard-cell geometry or timing libraries.
