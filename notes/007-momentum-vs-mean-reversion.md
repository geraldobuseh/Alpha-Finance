# Momentum vs mean reversion

## What did I think before?

A falling price might look cheap, and a rising price might look attractive. Neither
observation alone establishes what happens next.

## What is the concept?

Momentum hypothesizes continuation: recent relative winners keep outperforming.
Mean reversion hypothesizes reversal: a deviation from a reference level unwinds.
They make opposite directional assumptions about persistence versus reversal,
although different lookback periods can produce both effects in the same market.
A moving average is a reference, not intrinsic value or a promised destination.

PQL-023 uses the arithmetic mean of the latest 20 completed session closes before
the UTC decision day. The current quote is excluded from the average. Buy strictly
below MA * 0.95 while flat; sell the entire holding at or above MA; otherwise hold.
Entry spends available cash including commission and adverse slippage. It uses one
configured symbol, fractional shares, no borrowing, no shorts, and no adding to an
existing position. Insufficient cash for commission means no entry; missing quote
or fewer than 20 bars is an explicit error. The caller must supply complete session
history, consistent corporate-action adjustments, and a current executable quote.

## Why does Personal Quant Lab need it?

These are competing hypotheses to test on consistent data against owning SPY.
Synthetic tests establish that rules execute correctly; they do not establish an
investment edge. Compare returns, excess return, drawdown, volatility, turnover,
fees, slippage and trade counts with explicit assumptions and out-of-sample data.

## What failure would occur if we misunderstood it?

A declining business can keep falling. The average can fall toward the price,
triggering an exit at a loss without price recovery. This initial rule has no stop
loss or maximum holding time. Trend persistence can hurt reversal strategies;
choppy reversals can hurt momentum. Costs can erase apparent gains. Using future
closes or tuning thresholds on the evaluation sample would bias the experiment.

## What tiny example makes it intuitive?

With twenty closes of $100, MA is $100. A current quote of $94 triggers entry;
$95 does not. While holding, $99 means hold and $100 means exit. If MA later falls
to $90, a $90 exit can lose money relative to the $94 entry, even before costs.
Momentum instead favors recent winners on its ranking horizon, expecting strength
to persist rather than buying weakness in anticipation of reversal.

## What should I remember six months from now?

Continuation and reversal are hypotheses about a specified horizon and market
regime. Correct implementation is not evidence of profitability. Record the
lookback, thresholds, sizing, timing and costs, then test against SPY without leakage.
