# Market friction

## What did I think before?

If a strategy buys at $100 and sells at $100, it breaks even. That is true only when
execution and transaction costs are zero.

## What is the concept?

- Spread is the difference between the best quoted ask and bid. Buying at the ask
  and selling at the bid can cost money even when the midpoint does not move.
- Slippage is the difference between a reference price and the actual fill. It can
  be favorable or adverse; our conservative fake assumes adverse slippage only.
- Liquidity describes the ability to trade size promptly without substantially
  moving the price. A displayed quote does not guarantee that all shares fill there.
- Commissions are explicit broker charges. Our model uses one fixed amount per
  successful full fill, not a per-share or percentage commission schedule.
- Turnover measures trading activity relative to portfolio size. Conventions vary;
  a clearly stated gross convention is total bought plus sold notional divided by
  average portfolio value over the measured period. More trading repeats friction.

## Why does Personal Quant Lab need it?

Strategies must earn enough to cover their trading costs. Compare strategies and
SPY using explicit, consistent cost assumptions. Zero-cost simulations are useful
controls, not a claim that real trading is free.

## What failure would occur if we misunderstood it?

A high-turnover strategy can look profitable before costs and lose afterward.
Charging slippage both in the fill price and as a fee double-counts it. Checking
buy affordability against the reference quote can approve a fill the account
cannot afford after slippage and commission.

## What tiny example makes it intuitive?

One basis point is 0.01%; five basis points is 0.05%. At a $100 reference quote,
5bps adverse slippage fills a buy at $100.05 and a sell at $99.95.

Starting with $1,000, buy two shares with a $1 commission: cash becomes $798.90
and average cost becomes $100.55, including the commission. Sell both with the
same reference quote, slippage and commission: cash becomes $997.80 and realized
P&L is -$2.20. The loss is $0.20 of slippage plus $2 of commissions.

For our fixed-bps model, buy = quote + quote*(bps/10000), and sell uses subtraction.
The configured commission is recorded in transaction fees. Slippage is already
embedded in the recorded fill price and must not be charged again. Replay uses
recorded fills/fees, not today's configuration.

## What should I remember six months from now?

Costs are assumptions to record and stress-test, not constants established by a
backtest. This deterministic model has no order book, volume-dependent impact,
partial fills, tick-size rounding or separate spread charge. Fixed slippage can
approximate combined execution friction, but it does not simulate liquidity.
Both commission and slippage can be explicitly zero. Positive slippage that cannot
be represented in the execution price rejects rather than silently becoming free.
