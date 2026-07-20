# OpenRCX build boundary

OpenRCX links against `opendb`, OpenSTA Tcl support, and OpenROAD build definitions. It is
therefore built as the `rcx` module in the pinned OpenROAD source tree, not as an independent
binary. MFE uses the resulting OpenROAD executable only to read MFE-exported LEF/DEF geometry,
run `extract_parasitics`, and write SPEF.

MFE placement, CTS, global routing, and detailed routing remain native MFE responsibilities.
