# PQL-031 Architecture Review

Status: passed

The scorecard belongs in `analytics` as a deterministic summary over existing
portfolio and return primitives. The implementation keeps strategy ownership clean:
strategies do not mutate portfolios, and the scorecard consumes a terminal
`PortfolioSnapshot`, observed strategy equity, and aligned SPY equity.

Required invariants are represented in the builder:

- Strategy and SPY curves are nonempty, strictly increasing, timestamp-aligned,
  and nonnegative.
- Strategy equity starts at the portfolio starting cash.
- Terminal ledger transactions belong to the portfolio, are ordered, and stay
  inside the reporting window.
- Each strategy observation is checked against a replayed transaction prefix.
  Cash-only observations must equal replayed cash; open-position observations
  cannot be below replayed cash.
- Turnover, fees, trade count, total return, SPY return, excess return,
  drawdown, volatility, and simplified Sharpe are calculated from one window.

Final rereview found no remaining material findings. Open-position marks remain
trusted caller input until typed daily valuations are connected.
