# Alpha Finance - A Personal Quant Lab

A C++20 financial analysis system for testing investment edge through disciplined experimentation.

## Quick Start

### Prerequisites

- **CMake** 3.20 or later
- **Ninja** build system
- **C++20** capable compiler:
  - MSVC 19.30+ (Visual Studio 2022)
  - GCC 11+
  - Clang 14+

### Local PostgreSQL

Install/start Docker Desktop with Linux containers and Docker Compose. From the
repository root, copy the environment template once:

```powershell
Copy-Item .env.example .env
```

Set `POSTGRES_PASSWORD` in `.env` to your own local password. Keep an existing `.env`
when returning to the project; it is ignored by Git. Then start the database:

```powershell
docker compose config --quiet
docker compose up -d --wait postgres
docker compose ps
docker compose exec postgres psql -U pql -d personal_quant_lab -c "SELECT current_database();"
```

Host applications connect to **127.0.0.1:5432**, database **personal_quant_lab**, user
**pql**, using the password from `.env`. If port 5432 is occupied, change
`POSTGRES_PORT` in `.env` and use that port in the application. No host PostgreSQL
client installation is required for the query above.

The single `postgres` service uses the official PostgreSQL 17 image, a TCP readiness
check, and the `postgres_data` named volume. The image initializes the database and
credentials only when the volume is empty; changing `.env` later does not change an
existing database password. See the [official image documentation](https://hub.docker.com/_/postgres).

To stop and remove the container while retaining the database volume:

```powershell
docker compose down
```

Starting with `docker compose up -d --wait postgres` reuses that volume. Do not add
`--volumes` to the stop command if you want to retain data. This provisions the local
database only; app schema, migrations and engine persistence are separate tickets.

### Build Instructions

#### Configure the build:
```bash
cmake -S . -B build -G Ninja
```

This command:
- `-S .` — source directory (current)
- `-B build` — build directory
- `-G Ninja` — use Ninja generator

#### Build the project:
```bash
cmake --build build
```

#### Run tests:
```bash
ctest --test-dir build --verbose
```

### Clean Build

To remove all build artifacts:
```bash
rm -rf build        # macOS/Linux
rmdir /s build      # Windows PowerShell
```

## Project Structure

```
personal-quant-lab/
├── CMakeLists.txt          Main CMake configuration
├── README.md               This file
├── .gitignore              Git ignore rules
├── src/
│   ├── CMakeLists.txt      Source configuration
│   └── main.cpp            Minimal executable entry point
├── test/
│   ├── CMakeLists.txt      Test configuration (GoogleTest)
│   └── test_smoke.cpp      Smoke test suite
└── build/                  Generated build artifacts (after cmake)
```

## Architecture

### Build System

- **CMake** 3.20+: Modern, target-based architecture
- **Ninja**: Fast, parallel builds
- **GoogleTest**: Unit testing framework (auto-fetched via FetchContent)
- **CTest**: Test runner integration

### Compiler Configuration

**MSVC (Windows)**:
- `/W4` — Enable all warnings
- `/WX` — Treat warnings as errors
- `/wd4068` — Suppress "unknown pragma" for external deps
- `/wd4996` — Suppress deprecated warnings for stdlib

**GCC/Clang**:
- `-Wall -Wextra -Wpedantic` — Enable all warnings
- `-Werror` — Treat warnings as errors

### C++ Standard

- **C++20** (required)
- No extensions (`CMAKE_CXX_EXTENSIONS OFF`)
- Standard features only (no compiler-specific code)

## Development

### Adding New Source Files

1. Add `.cpp` files to `src/`
2. Update `src/CMakeLists.txt` to include new sources
3. Rebuild: `cmake --build build`

### Adding New Tests

1. Add test `.cpp` files to `test/`
2. Update `test/CMakeLists.txt` to register tests
3. Run tests: `ctest --test-dir build --verbose`

### Compiler Flags

All compiler flags are set in the root `CMakeLists.txt`:
- Global warning levels
- C++ standard
- Platform-specific settings

Modify these to adjust build strictness.

## Continuous Integration

When building in CI/CD:

```bash
# Fresh clone
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --verbose
```

The build will fail if:
- Any source file fails to compile
- Any warning is generated (treated as error)
- Any test fails

## PQL Architecture

Personal Quant Lab follows a disciplined design:

1. **Betting Framework** (PQL-001): Define falsifiable hypotheses
2. **Build System** (PQL-002): Production-grade C++20 foundation
3. **Portfolio Tracking** (PQL-003): Track real and paper positions
4. **Strategy Framework** (PQL-004): Run systematic strategies
5. **Backtesting** (PQL-005): Test strategies deterministically
6. **Analytics** (PQL-006): Analyze results vs. benchmarks

This ticket (PQL-002) provides the foundation for all domain code in later tickets.

## Status

The [Alpha Vantage integration](docs/real-market-data.md) ingests daily OHLCV for
eight supported symbols with idempotent storage, bounded retries and JSON results (PQL-014).

The [market-data provider interface](docs/market-data-provider.md) defines the
provider-independent engine boundary and validated daily-bar types (PQL-012).
The [fake provider and offline simulation](docs/fake-market-data.md) supply known
prices for deterministic tests without networking (PQL-013).

### Database schema (PQL-010)

The [C++ persistence layer](docs/persistence.md) provides libpqxx-backed repository
interfaces and a disposable Docker integration-test runner (PQL-011).

The v0.1 PostgreSQL migration defines the 11 domain tables and their constraints.
See [migration and test commands](db/README.md) and the
[column-by-column constraint rationale](db/schema.md). Transactions are append-only;
positions and snapshots are rebuildable projections. The optional PQL-011
persistence target implements repository access and ledger reconstruction.

Paper simulations can now use [the SPY buy-and-hold control](docs/bets/PQL-020.md).
`PaperPortfolioPair::create` starts both accounts with $1,000 and completes the
control's fractional SPY purchase using configured commission and slippage. The
control stays held while paper orders execute independently. This is an in-memory,
price-based benchmark; dividends and persistent pair registration are not included.

The [weekly DCA strategy](docs/bets/PQL-021.md) proposes a $25 total-budget SPY buy
each supplied UTC Monday, using existing cash and configured trading costs. It can
run against the paired buy-and-hold control without adding deposits.

The [20-day Momentum strategy](docs/bets/PQL-022.md) ranks a configured universe
using21 aligned prior-session closes and rebalances into the top two each UTC
Monday. Use `executePaperBatch` for its ordered sells and buys: a rejected leg
rolls back the entire rebalance. Synthetic fixtures verify ranking and weekly costs.

---

For questions or issues, see `.codex/AGENTS.md` for project governance.
