# Windows Smoke Tests

These checks exercise the real Win32 backend against local processes. They are
safe for developer machines because write tests target only controlled memory
inside the current test process in the unit suite; manual commands below are
read-only.

## Automated

Run:

```powershell
cmake --build build
build\unittest\Debug\memmy_test.exe
```

The Windows-only unit tests cover:

- default backend capability bits and read/write callbacks
- current-process `ReadProcessMemory` and `WriteProcessMemory`
- current-process module and region enumeration
- current-process value and pattern scans over controlled memory
- CLI runner smoke coverage for `peek`, `poke --dry-run`, real `poke`,
  `pscan`, and `scan` against controlled current-process memory

## Manual Read-Only CLI Checks

From the repository root, after building:

```powershell
$memmy = ".\build\cmd\memmy\Debug\memmy.exe"
& $memmy procs
$pid = $PID
& $memmy mods --pid $pid
& $memmy regions --pid $pid
```

Expected:

- `procs` prints stable PID, architecture, and process name rows; JSON mode also
  includes `path` and `pointer_width` when Windows allows querying that process.
- `mods --pid $PID` prints at least the current executable module with non-zero
  base and size.
- `regions --pid $PID` prints committed/reserved/free regions. If a region end
  would overflow, the CLI must fail with `overflow` instead of wrapping.

## Platform-Dependent Checks

- Access denied: querying protected system processes may return
  `access_denied` with a non-zero Windows `os_code`; this depends on elevation
  and local policy.
- WOW64: on a 64-bit Windows host, run the CLI from a 64-bit build against both
  a native 64-bit process and a WOW64 32-bit process. The process/module/region
  commands should report the target pointer width accurately where Windows
  allows the query.
- Unsupported cross-bitness: a 32-bit Memmy build running on 64-bit Windows may
  return `unsupported` with the diagnostic `32-bit Memmy cannot inspect a
  64-bit target process` when opening a native 64-bit target.
