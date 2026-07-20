# Developer Preview v0.1 release boundary

## Included

- Portable core CMake build and tests.
- Relative ASAP7 and Sky130 PDK manifest paths.
- Read-only evidence-first AI CLI with offline mode and an OpenAI-compatible provider contract.
- MFE routing, DEF, SPEF, timing evidence, and reviewed ECO planning.

## Explicitly not included

- Foundry signoff or tapeout qualification.
- Automatic ECO mutation.
- Mixed-layer via insertion, DRC repair, real standard-cell GDS/OASIS references, or LVS closure.
- Bundled third-party tools, PDK installations, or model credentials.

## Release checklist

1. Choose a repository license and add `LICENSE`.
2. Add a `NOTICE` file listing third-party sources and their licenses.
3. Run the 17-test core suite on a clean clone.
4. Build optional tools and run the Sky130 demonstration flow on a documented host.
5. Tag the first release as `v0.1.0-dev-preview`.
