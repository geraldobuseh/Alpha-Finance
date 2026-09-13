-- Extend existing projections without assigning invented returns to legacy rows.
SET LOCAL search_path = public, pg_catalog;
ALTER TABLE portfolio_snapshots
    ADD COLUMN session_date date,
    ADD COLUMN price_source nonblank_text,
    ADD COLUMN daily_return finite_amount CHECK (daily_return >= -1),
    ADD COLUMN cumulative_return finite_amount CHECK (cumulative_return >= -1),
    ADD COLUMN previous_as_of event_time,
    ADD CONSTRAINT daily_snapshot_provenance CHECK (
        (session_date IS NULL AND price_source IS NULL AND daily_return IS NULL
         AND cumulative_return IS NULL AND previous_as_of IS NULL)
        OR (session_date IS NOT NULL AND price_source IS NOT NULL
            AND session_date BETWEEN DATE '0001-01-01' AND DATE '9999-12-31'
            AND session_date = (as_of AT TIME ZONE 'UTC')::date
            AND (previous_as_of IS NULL OR previous_as_of < as_of)
            AND (previous_as_of IS NOT NULL OR daily_return IS NULL))),
    ADD CONSTRAINT daily_snapshot_previous FOREIGN KEY (portfolio_id, previous_as_of)
        REFERENCES portfolio_snapshots(portfolio_id, as_of);
CREATE UNIQUE INDEX portfolio_snapshots_session_idx
    ON portfolio_snapshots(portfolio_id, session_date) WHERE session_date IS NOT NULL;

CREATE TABLE daily_valuation_marks (
    portfolio_id bigint NOT NULL,
    as_of event_time NOT NULL,
    symbol text COLLATE "C" NOT NULL REFERENCES assets(symbol),
    close positive_amount NOT NULL,
    PRIMARY KEY (portfolio_id, as_of, symbol),
    FOREIGN KEY (portfolio_id, as_of) REFERENCES portfolio_snapshots(portfolio_id, as_of)
);
COMMENT ON TABLE daily_valuation_marks IS
    'Raw USD closes actually used. Source/session are on parent snapshot; ledger_sequence reconstructs quantities.';
COMMENT ON COLUMN portfolio_snapshots.daily_return IS
    'Fractional simple return from previous supplied session; NULL for first observation or zero denominator.';
COMMENT ON COLUMN portfolio_snapshots.cumulative_return IS
    'Fractional return from immutable starting cash, NULL when starting cash is zero. No external cash flows.';
