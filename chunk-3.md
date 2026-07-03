# Chunk 3: Windows Inventory Backend And Initial CLI

## Goal

Provide the first real Windows backend slice and the initial usable CLI
commands: `memmy --help`, `memmy procs`, and `memmy mods --pid <pid>`.

## Spec Coverage

- Section 6: native OS calls stay under `memmy/src/platform/win32/`.
- Sections 7-9: real process, module, and region data shapes.
- Section 15.1-15.4: CLI shape, global options, `procs`, and `mods`.
- Section 18.1-18.3: Windows APIs, access mapping, and region mapping.
- Section 22 milestone 1.

## Steps

1. Implement `Memmy_Context_InitDefault` so Windows hosts install the Win32
   backend and unsupported hosts return `Memmy_Status_Unsupported` until their
   chunks are complete.
2. Add Win32 backend source files for process, module, memory, and region
   functions under `memmy/src/platform/win32/`.
3. Implement backend capabilities for:
   - `Memmy_BackendCap_ListProcs`
   - `Memmy_BackendCap_ListModules`
   - `Memmy_BackendCap_ListRegions`

   Wire read/write capability fields and backend stubs only as needed for the
   compiled backend shape. Full Win32 read behavior is completed in Chunk 4,
   and full Win32 write behavior is completed in Chunk 5.
4. Implement process enumeration using documented Win32 APIs from the spec.
5. Implement process open/close with correct access mapping and harmless
   repeated close behavior.
6. Implement module enumeration using Toolhelp APIs for the initial version.
7. Implement region enumeration and map Windows protection/state flags to
   `Memmy_RegionAccess` and `Memmy_RegionState`.
8. Replace the current no-op CLI with command dispatch and global option
   parsing.
9. Keep command execution data-oriented: command handlers should collect typed
   result data and pass it to text renderers, rather than interleaving backend
   calls with direct printing. Chunk 8 adds JSON/JSONL renderers over the same
   result data.
10. Add help text that teaches the command shape and points to address
   expression syntax even before later commands are implemented.
11. Implement `procs` text output and `--filter`.
12. Implement `mods --pid <pid>` text output and `--filter`.
13. Check backend capabilities before opening processes when possible.
14. Map failures to the exit codes defined in the spec.

## Tests

1. Unit tests for CLI argument parsing without touching real processes.
2. Unit tests with the test backend for `procs` and `mods` formatting.
3. Windows integration smoke test:
   - `memmy --help`
   - `memmy procs`
   - `memmy mods --pid <current-process-pid>`
4. Unit tests for access mapping and unsupported capability diagnostics where
   practical.

## Done When

- `cmd_memmy` builds and emits an executable named `memmy`.
- The first CLI milestone works on Windows.
- Non-platform core and CLI files do not include Windows headers.
- Later chunks can reuse the same command parser and backend setup.
