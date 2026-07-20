# MFE Studio: Production Agentic EDA Architecture

## Product boundary

MFE Studio is an RTL-to-GDSII orchestration platform. MFE owns the project database,
PDK compiler, physical optimization engines, artifacts, diagnostics, experiments, and
approval policy. External projects are replaceable, pinned adapters used for specialist
capabilities that MFE does not yet implement natively.

## Authoritative data model

The flow never treats tool logs as the source of truth. Every stage writes structured
records linked by stable entity identifiers:

`RTL symbol -> synthesized cell -> placed instance -> routed net -> extracted parasitic -> timing path -> finding`

The record set contains:

- project and PDK manifests;
- RTL/SDC/configuration locations indexed by language-server/AST adapters;
- design entities, geometry, constraints, metrics, and artifacts;
- findings with rule provenance, severity, source location, and downstream impact;
- approved change sets, isolated experiments, and before/after measurements.

## AI boundary

The agent reads the evidence graph through narrow query tools. It may propose a change set
to RTL, constraints, flow configuration, or a physical ECO. The project policy blocks PDK
edits and prohibits applying any change without an engineer approval record. Each approved
change triggers a bounded rerun from the earliest affected stage.

## Engine ownership

| Capability | Initial implementation | Long-term MFE direction |
|---|---|---|
| RTL AST/LSP | Verible adapter | MFE entity/index service |
| Logic synthesis | Yosys adapter | Optional native mapping research |
| PDK compiler | MFE | MFE |
| Floorplan/place/thermal/CTS/route | MFE | MFE |
| RC extraction | OpenRCX adapter | Geometry-aware MFE extractor |
| Static timing | OpenSTA adapter | MFE timing graph and Liberty reader |
| DRC/LVS/layout inspection | KLayout/Magic adapters | MFE rule evidence and signoff adapters |
| GDSII/OASIS | KLayout/MFE streaming adapter | Native streaming writer |

## Delivery sequence

1. Project manifest, pinned toolchain, design-evidence graph, and flow runner.
2. RTL/SDC indexes and external synthesis/timing/verification adapters.
3. Native MFE physical stages wired into one deterministic run.
4. Impact analysis, experiment runner, and approval-gated AI actions.
5. Backend APIs and CLI, benchmark regression, PDK qualification, packaging, and release controls.
6. Build a graphical engineer interface only after the backend contracts and workflows are validated.

## Production controls

- Pin every external source revision and record binary/version data in each run.
- Run tools in isolated work directories with explicit input/output allowlists.
- Preserve immutable artifacts and hashes for every release candidate.
- Report signoff status only from a successful approved verification adapter run.
- Keep PDK data immutable within an experiment; a PDK update is a separately reviewed release.
