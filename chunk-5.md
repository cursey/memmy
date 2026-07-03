# Chunk 5: Absolute Peek

## Goal

Implement remote memory reads and the v0 `peek` command using absolute
`--addr` input only.

## Spec Coverage

- `spec-v0.md` section 5: absolute address parsing.
- `spec-v0.md` section 9: `Memmy_Process_Read`.
- `spec-v0.md` section 10: type sizes and pointer width.
- `spec-v0.md` section 12.4: `peek`.
- `spec-v0.md` section 13: address and value formatting.
- `spec-v0.md` section 14: Windows read backend.
- `spec-v0.md` section 16 milestone 4.

## Steps

1. Implement `Memmy_Process_Read` as process-bound backend dispatch.
2. Extend the test backend read behavior to support:
   - successful full reads
   - partial reads
   - unreadable ranges
   - configurable base address for the fake memory buffer
3. Implement Windows backend read with `ReadProcessMemory` or the chosen v0
   read API.
4. Map backend read failures to:
   - `Memmy_Status_Unreadable`
   - `Memmy_Status_PartialRead`
   - `Memmy_Status_AccessDenied`
   - `Memmy_Status_PlatformError`
5. Add read-size resolution for parsed types:
   - fixed scalar size from `Memmy_Type.fixed_size`
   - `ptr` uses target process pointer width
   - `bytes`, `str`, and `wstr` require `--count`
6. Implement `memmy peek --pid <pid> --addr <addr> --type <type>`.
7. Implement variable-width forms:
   - `memmy peek --pid <pid> --addr <addr> --type bytes --count <count>`
   - `memmy peek --pid <pid> --addr <addr> --type str --count <count>`
   - `memmy peek --pid <pid> --addr <addr> --type wstr --count <count>`
8. Validate `--count` with the v0 size parser and reject missing/extra count
   arguments according to the type.
9. Format text output in the v0 shape, including fixed-width lowercase hex
   addresses.
10. Decode or validate `str` and `wstr` reads according to the v0 encoding
    requirements; invalid encodings return `Memmy_Status_InvalidEncoding`.
11. Request only `Memmy_ProcessAccess_Read` and require only
    `Memmy_BackendCap_Read`.
12. Do not support `--expr`, module-relative input, pointer chains, or the
    `addr` command.

## Tests

1. Unit tests for `Memmy_Process_Read` dispatch through the test backend.
2. Unit tests for full read, partial read, unreadable range, and access-denied
   mapping.
3. Unit tests for peek read-size resolution for all fixed-size types, `ptr`,
   and variable-width types.
4. CLI tests with the test backend for scalar, pointer, `bytes`, `str`, and
   `wstr` peeks.
5. CLI tests proving variable-width types require `--count`.
6. CLI tests proving `--addr` rejects expression-like input.
7. Windows smoke test reading from a known current-process address where safe.

## Done When

- `peek` satisfies the v0 absolute-address text behavior.
- Read failures and encoding failures return stable `Memmy_Status` values.
- The build and unit tests pass before starting Chunk 6.
