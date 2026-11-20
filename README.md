# libzugodb

A small embedded columnar SQL engine in C++20, the engine of the R package
[zugodb](https://github.com/pedrobtz/zugodb).

libzugodb began as a fork of [SlothDB](https://github.com/SouravRoy-ETL/slothdb)
at commit `8180f21` (v0.2.7+, 2026-06-03), by the SlothDB Contributors, under
the MIT licence. Upstream has had no commit since, so it is maintained here as
a project of its own. Defects found here are reported upstream
(SouravRoy-ETL/slothdb#29 to #42, with fixes in #43 to #48).

## How it differs from SlothDB

- **It refuses what it cannot do right.** Where SlothDB returned a wrong value
  with no error, libzugodb raises one: `ROLLBACK` (there is no undo) and
  `STRFTIME` without a format, among others.
- **It reads and writes no file from SQL.** SlothDB's Parquet, CSV, JSON,
  SQLite, Excel, Avro and Arrow readers are removed, with the zstd, snappy
  and FastPFor libraries they used; `read_csv()`, `COPY` and the other file
  routes are refused. Files reach the engine as Arrow C streams, through
  zugodb.
- **It is offline and loads no code.** A remote path (`http://`, `https://`,
  `s3://`) is an error, and the HTTP client, the extension loader and the
  natural-language `ask` module (which downloads a model) are removed.
- **It is quiet and bounded.** Nothing is written to `stderr`, nothing calls
  `abort()` or `rand()`, and one process-wide cap limits every thread pool.
- **It builds with MinGW-w64**, as R's Rtools uses on Windows.
- **Its C++ namespace is `zugodb::`.**
- **It is only the engine.** SlothDB's C API, Python binding, website, WASM
  playground, packaging and benchmarks are not part of this repository.

`CHANGELOG.md` lists each change. The plan is in the R package's design: the
file readers move to separate packages linked over Arrow C streams, and other
packages extend the engine through registered table sources and scalar
functions.

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
build/test/slothdb_tests          # the doctest suite
```

`-DSLOTHDB_BUILD_SHELL=ON` adds the `slothdb` shell; `-DSLOTHDB_SANITIZERS=ON`
builds with AddressSanitizer and UBSan. The CMake options, the shell and the
library keep SlothDB's names for now.

SlothDB's own README is kept in
[docs/SLOTHDB-README.md](docs/SLOTHDB-README.md); its performance claims are
SlothDB's.

## Licence

MIT; see `LICENSE`. Third-party code under `third_party/` keeps its own
licence.
