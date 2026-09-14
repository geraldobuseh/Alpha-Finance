# PQL-026 C++20 Review

C++ REVIEW PASSED after numerical findings were fixed and independently re-reviewed.

Daily/cumulative operation order and error translation preserve existing persistence
semantics. Pure value inputs/outputs introduce no ownership or lifetime hazards.

Resolved HIGH: log1p(relative_change) near -1 loses remaining-wealth precision because
subtraction has already rounded away its gross ratio. Restrict log1p to nearby endpoints
(abs(change)<=0.5); use a direct normal gross ratio otherwise.

Resolved additional extreme case: a positive subnormal gross ratio may round heavily
without becoming zero. Only use direct-ratio logarithm for finite ratios >= min normal;
use endpoint logarithm differences for overflow or subnormal ratios. Tests cover
2/1e16, 3/2^54 and denorm_min/1.5 over long horizons.

Use expm1 to retain small annualized changes. Reject infinities, erased nonzero returns
and false total losses. ACT/365 Fixed and duration validation remain explicit. No
remaining material issues. Reviewer inspected source/tests; execution is in validation.md.
