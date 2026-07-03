# Chunk 4: Type, Value, And Pattern Parsing

## Goal

Implement the v0 type parser, typed value encoder, and byte-pattern parser used
by `peek`, `poke`, `scan`, and `pscan`.

## Spec Coverage

- `spec-v0.md` section 10: required value types, `Memmy_Type`,
  `Memmy_Value`, and `Memmy_Pattern`.
- `spec-v0.md` section 12.4-12.7: command inputs for typed values and patterns.
- `spec-v0.md` section 15: typed value parsing, pattern parsing, and wildcard
  rejection tests.

## Steps

1. Define:
   - `Memmy_TypeKind`
   - `Memmy_Type`
   - `Memmy_Value`
   - `Memmy_PatternByte`
   - `Memmy_Pattern`
   - `Memmy_PatternParseFlags`
2. Implement `Memmy_Type_Parse` for:
   - `u8`, `i8`
   - `u16`, `i16`
   - `u32`, `i32`
   - `u64`, `i64`
   - `f32`, `f64`
   - `ptr`
   - `bytes`
   - `str`
   - `wstr`
3. Set `fixed_size` from the parsed type alone:
   - fixed scalar sizes for numeric types
   - `0` for `ptr` before target pointer width is applied
   - `0` for `bytes`, `str`, and `wstr`
4. Implement `Memmy_Value_Parse`:
   - integer exact bounds and overflow checks
   - pointer-width-aware `ptr` encoding
   - little-endian scalar encoding
   - `f32` and `f64` decimal parsing
   - `bytes` through `Memmy_Pattern_Parse` with wildcards rejected
   - UTF-8 `str` encoding without a trailing NUL
   - UTF-16LE `wstr` encoding without a trailing NUL
5. Implement `Memmy_Pattern_Parse`:
   - lowercase or uppercase hex bytes
   - whitespace-separated byte tokens
   - `??` wildcard tokens only when
     `Memmy_PatternParseFlag_AllowWildcards` is set
   - precise parse errors for invalid byte and wildcard syntax
6. Add a small internal matcher helper if useful for scanner tests, but keep the
   public API limited to the v0 functions unless a public matcher already exists.
7. Do not implement address expressions, constant expressions, or
   module-relative syntax in this chunk.

## Tests

1. Unit tests for every accepted `Memmy_Type_Parse` spelling.
2. Unit tests for rejected type names and error context `type`.
3. Unit tests for integer value parsing at min/max bounds for every signed and
   unsigned integer type.
4. Unit tests for integer overflow, underflow, invalid syntax, and wrong signs.
5. Unit tests for pointer encoding at 32-bit and 64-bit widths.
6. Unit tests for `f32` and `f64` parsing and byte encoding.
7. Unit tests for `bytes`, `str`, and `wstr` encoding.
8. Unit tests for pattern parsing with exact bytes, wildcards, invalid tokens,
   and wildcard rejection when flags are not set.

## Done When

- Parsing and encoding is shared by later `peek`, `poke`, `scan`, and `pscan`
  commands.
- `bytes` values reject wildcards while `pscan --pattern` can allow them.
- The build and unit tests pass before starting Chunk 5.
