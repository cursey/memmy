# Chunk 5: Value Encoding And Memory Write

## Goal

Add typed value parsing/encoding, direct memory writes, and the `poke` command
with dry-run support.

## Spec Coverage

- Section 10.2: write behavior.
- Section 12.1: value parsing and encoding.
- Section 15.7: `poke`.
- Section 22 milestone 3.

## Steps

1. Implement `Memmy_Value` and `Memmy_Value_Parse`.
2. Support scalar integer values with decimal and hexadecimal forms plus
   optional leading sign.
3. Enforce exact range fitting for every integer type.
4. Encode `ptr` using the target process pointer width and reject values that
   do not fit.
5. Support initial decimal floating-point parsing for `f32` and `f64`.
6. Encode all scalar values little-endian.
7. Parse `bytes` values through `Memmy_Pattern_Parse` with wildcards disabled.
8. Encode `str` as UTF-8 without a trailing NUL.
9. Encode `wstr` as UTF-16LE without a trailing NUL.
10. Implement `Memmy_Process_Write` backend dispatch status behavior:
    - `Memmy_Status_Unwritable` for protection-related failures when known
    - `Memmy_Status_AccessDenied` for insufficient rights when known
    - `Memmy_Status_PlatformError` for other native failures
11. Implement Windows backend write using direct write APIs without changing
    remote memory protection.
12. Implement `poke --dry-run` so it reads the old value, formats old/new
    values, and performs no write.
13. Implement normal `poke` so it reads old value first for display, then
    writes the encoded new value.
14. Keep `poke` independent of region enumeration in the initial version.
15. Keep command execution data-oriented so Chunk 8 can add JSON/JSONL
    renderers without rewriting backend and parsing logic.

## Tests

1. Unit tests for every type spelling accepted by `Memmy_Type_Parse`.
2. Unit tests for integer value parsing, exact bounds, and overflow/underflow.
3. Unit tests for pointer encoding at 32-bit and 64-bit widths.
4. Unit tests for byte pattern value parsing with wildcards rejected.
5. Unit tests for UTF-8 and UTF-16LE string encoding.
6. Test-backend write tests, including partial write and unwritable cases.
7. CLI tests for dry-run proving memory is unchanged.
8. CLI tests for normal `poke` proving memory changes in the test backend.

## Done When

- `poke` and `poke --dry-run` satisfy the spec text-output behavior.
- All value parsing is shared by `poke` and the later `scan` command.
- The build and unit tests pass.
