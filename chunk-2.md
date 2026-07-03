# Chunk 2: Numeric Address, Size, And Range Helpers

## Goal

Implement the v0-only numeric input helpers for absolute addresses, sizes, and
single half-open ranges.

This replaces the full-spec address-expression and range-expression direction.
v0 accepts unsigned integer tokens only and scan commands accept exactly one
explicit range per invocation.

## Spec Coverage

- `spec-v0.md` section 5: address and size input syntax.
- `spec-v0.md` section 6: `Memmy_Range` and range construction rules.
- `spec-v0.md` section 7: parse and overflow errors with context.
- `spec-v0.md` section 12: CLI range option forms used by `scan` and `pscan`.
- `spec-v0.md` section 15: address parsing, size parsing, and range validation
  tests.

## Steps

1. Add `Memmy_Range` to the public API:
   - `Memmy_Addr start`
   - `Memmy_Addr end`
2. Implement:
   - `Memmy_ParseAddress`
   - `Memmy_ParseSize`
   - `Memmy_Range_FromStartEnd`
   - `Memmy_Range_FromStartLength`
3. Accept only unsigned decimal and `0x`/`0X` hexadecimal integer tokens.
4. Reject signs, whitespace-padded partial parses, parentheses, arithmetic,
   module names, and any trailing characters.
5. Use checked accumulation so integer overflow returns
   `Memmy_Status_Overflow`.
6. Return `Memmy_Status_ParseError` for invalid syntax and fill `Memmy_Error`
   with:
   - `context = "address"` for `Memmy_ParseAddress`
   - `context = "range"` for `Memmy_ParseSize` when it is used for CLI ranges,
     or a caller-provided context if a helper already exists
   - input span information where practical
7. Implement range validation:
   - `start <= end`
   - `end = start + length`
   - `start + length` must not overflow
   - zero-length ranges are valid
8. Add CLI-parser helper logic, but not full `scan`/`pscan` commands yet, for
   validating exactly one range form:
   - `--start <addr> --end <addr>`
   - `--start <addr> --length <size>`
9. Ensure the helper rejects missing `--start`, simultaneous `--end` and
   `--length`, and neither `--end` nor `--length`.

## Tests

1. Unit tests for accepted addresses:
   - `0x000001d856780004`
   - `0X1000`
   - `4096`
2. Unit tests for rejected addresses:
   - `-1`
   - `+1`
   - `0x1000+4`
   - `(0x1000)`
   - `client.dll`
   - `0x`
   - empty input
3. Unit tests for decimal and hexadecimal overflow.
4. Unit tests for `Memmy_Range_FromStartEnd`, including equal start/end and
   rejected `end < start`.
5. Unit tests for `Memmy_Range_FromStartLength`, including zero length and
   overflow.
6. CLI parser tests for the two valid range option shapes and every invalid
   combination.

## Done When

- All v0 numeric helpers are implemented without introducing expression
  parsers.
- Range construction is shared by later `scan` and `pscan` work.
- The build and unit tests pass before starting Chunk 3.
