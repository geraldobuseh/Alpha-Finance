# PQL-027 Quantitative Correctness Review

QUANT REVIEW PASSED. Independent design and final implementation/documentation review
found no unresolved financial correctness issue.

Running peak P is the greatest prior observed value. Drawdown=(V-P)/P; maximum drawdown
is the most negative observation. A later recovery does not erase the loss. Peak1200,
trough900 gives (900-1200)/1200=-0.25. Peak must precede the losing trough.

Empty/all-zero baselines are undefined; positive singleton/flat/rising histories yield
zero. Positive peak followed by zero is -100%. Leading zeros provide no denominator.
Earliest tied observations are retained. Negative equity, invalid chronology and
unrepresentable declines reject explicitly. Historical observations assume comparable
funding/share basis; external-flow adjustment is not introduced.

With comparable periods/costs, B is a conditional downside-conscious preference:
11% return and -12% drawdown versus A at15% and -45%. It sacrifices4 return percentage
points for33 points less observed drawdown. Recovery requires13.6% versus81.8% growth.
Two metrics cannot prove superiority or future safety. Compare SPY, recovery time and
out-of-sample evidence. Daily sampling may miss intraday losses.
