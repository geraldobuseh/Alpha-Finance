\set ON_ERROR_STOP on
BEGIN;
SET LOCAL TIME ZONE 'UTC';
CREATE FUNCTION pg_temp.rejects(statement text, expected_state text) RETURNS void LANGUAGE plpgsql AS $$
BEGIN
    BEGIN
        EXECUTE statement;
    EXCEPTION WHEN OTHERS THEN
        IF SQLSTATE = expected_state THEN RETURN; END IF;
        RAISE EXCEPTION 'Expected %, received % for %: %', expected_state, SQLSTATE, statement, SQLERRM;
    END;
    RAISE EXCEPTION 'Unexpected success: %', statement;
END;
$$;

INSERT INTO assets(asset_id,symbol,name,currency) VALUES (101,'AAPL','Apple','USD'),(102,'SPY','Benchmark','USD');
INSERT INTO portfolios(portfolio_id,name,kind,currency,starting_cash) VALUES (101,'Test','simulated','USD',1000),(102,'Other','real','USD',1000);
INSERT INTO strategies(strategy_id,name,version) VALUES (101,'Test','v1');
INSERT INTO strategy_runs(run_id,portfolio_id,strategy_id,strategy_version,range_start,range_end,starting_capital,parameters,universe,fee_per_order,slippage_bps,adjustment,status)
VALUES (101,101,101,'v1','2026-01-01','2026-01-02',1000,'{}','["AAPL"]',0,0,'raw','planned');
INSERT INTO market_prices VALUES (101,'2026-01-01','synthetic','raw',100,120,90,120,0);
INSERT INTO orders(portfolio_id,order_id,asset_id,side,quantity,submitted_at,strategy_run_id)
VALUES (101,1,101,'Buy',2,'2026-01-01',101),(101,2,101,'Sell',1,'2026-01-01',NULL),(102,1,101,'Buy',2,'2026-01-01',NULL);
INSERT INTO executions VALUES (101,1,101,'Buy',2,'2026-01-01',100,0,'2026-01-01'),(101,2,101,'Sell',1,'2026-01-01',120,0,'2026-01-01');
INSERT INTO transactions VALUES (101,1,1,101,'AAPL','Buy',2,100,0,'2026-01-01'),(101,2,2,101,'AAPL','Sell',1,120,0,'2026-01-01');
INSERT INTO positions VALUES (101,101,1,100,20,'2026-01-01',2);
INSERT INTO portfolio_snapshots(portfolio_id,as_of,ledger_sequence,cash,market_value)
VALUES (101,'2026-01-01',2,920,120),(102,'2026-01-01',NULL,1000,0);
INSERT INTO benchmark_snapshots VALUES (101,'2026-01-01',102,'SPY','2026-01-01',1000,1000,'raw');
INSERT INTO daily_valuation_marks VALUES (101,'2026-01-01','AAPL',120);

-- Exercise NOT NULL for every required, non-generated column using valid rows.
DO $$
DECLARE c record; columns text; values_sql text;
BEGIN
    FOR c IN SELECT table_name,column_name FROM information_schema.columns
      WHERE table_schema='public' AND table_name <> 'schema_migrations'
      AND is_nullable='NO' AND is_generated='NEVER'
    LOOP
        SELECT string_agg(format('%I',column_name),',' ORDER BY ordinal_position),
               string_agg(CASE WHEN column_name=c.column_name THEN 'NULL' ELSE format('%I',column_name) END,',' ORDER BY ordinal_position)
          INTO columns,values_sql FROM information_schema.columns
          WHERE table_schema='public' AND table_name=c.table_name AND is_generated='NEVER';
        PERFORM pg_temp.rejects(format('INSERT INTO %I (%s) SELECT %s FROM %I LIMIT 1',c.table_name,columns,values_sql,c.table_name),'23502');
    END LOOP;
END;
$$;

-- Reusable domain edge cases, including PostgreSQL's special numeric values.
SELECT pg_temp.rejects(format('SELECT %L::positive_amount',v),'23514') FROM unnest(ARRAY['0','-1','NaN','Infinity','-Infinity']) v;
SELECT pg_temp.rejects(format('SELECT %L::nonnegative_amount',v),'23514') FROM unnest(ARRAY['-1','NaN','Infinity','-Infinity']) v;
SELECT pg_temp.rejects(format('SELECT %L::finite_amount',v),'23514') FROM unnest(ARRAY['NaN','Infinity','-Infinity']) v;
SELECT pg_temp.rejects(format('SELECT %L::event_time',v),'23514') FROM unnest(ARRAY['infinity','-infinity','2026-01-01 00:00:00.000001+00']) v;
SELECT pg_temp.rejects($s$SELECT ' '::nonblank_text$s$,'23514');
SELECT pg_temp.rejects($s$SELECT 'Short'::trade_side$s$,'23514');
SELECT pg_temp.rejects($s$INSERT INTO assets VALUES (103,'bad symbol','Bad','USD')$s$,'23514');
SELECT pg_temp.rejects($s$INSERT INTO assets VALUES (103,'AAPL','Duplicate','USD')$s$,'23505');
SELECT pg_temp.rejects($s$INSERT INTO assets VALUES (-1,'BAD','Bad','USD')$s$,'23514');
SELECT pg_temp.rejects($s$INSERT INTO assets VALUES (103,'EUR','Euro','EUR')$s$,'23514');
SELECT pg_temp.rejects($s$INSERT INTO market_prices SELECT * FROM market_prices$s$,'23505');
SELECT pg_temp.rejects($s$UPDATE market_prices SET asset_id=999$s$,'23503');
SELECT pg_temp.rejects($s$UPDATE market_prices SET low=110$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE market_prices SET adjustment='unknown'$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE portfolios SET kind='margin'$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE portfolios SET starting_cash=2000$s$,'23514');
SELECT pg_temp.rejects($s$INSERT INTO strategies VALUES (102,'Test','v1')$s$,'23505');
SELECT pg_temp.rejects($s$UPDATE strategy_runs SET strategy_version='v2'$s$,'23503');
SELECT pg_temp.rejects($s$UPDATE strategy_runs SET range_end='2025-01-01'$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE strategy_runs SET parameters='[]'$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE strategy_runs SET universe='[]'$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE strategy_runs SET universe='[1]'$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE strategy_runs SET slippage_bps=10001$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE strategy_runs SET status='unknown'$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE orders SET strategy_run_id=101 WHERE portfolio_id=102$s$,'23503');
SELECT pg_temp.rejects($s$UPDATE orders SET order_type='Limit' WHERE portfolio_id=102$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE orders SET status='Filled' WHERE portfolio_id=102$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE orders SET quantity=0 WHERE portfolio_id=102$s$,'23514');
SELECT pg_temp.rejects($s$INSERT INTO executions VALUES (102,1,101,'Sell',2,'2026-01-01',100,0,'2026-01-01')$s$,'23503');
SELECT pg_temp.rejects($s$INSERT INTO executions VALUES (102,1,101,'Buy',1,'2026-01-01',100,0,'2026-01-01')$s$,'23503');
SELECT pg_temp.rejects($s$INSERT INTO executions VALUES (102,1,101,'Buy',2,'2026-01-01',100,0,'2025-01-01')$s$,'23514');
SELECT pg_temp.rejects($s$INSERT INTO executions SELECT * FROM executions LIMIT 1$s$,'23505');
SELECT pg_temp.rejects($s$INSERT INTO transactions VALUES (102,1,1,101,'AAPL','Buy',2,100,0,'2026-01-01')$s$,'23503');
INSERT INTO executions VALUES (102,1,101,'Buy',2,'2026-01-01',100,0,'2026-01-01');
SELECT pg_temp.rejects($s$INSERT INTO transactions VALUES (102,1,1,101,'SPY','Buy',2,100,0,'2026-01-01')$s$,'23503');
SELECT pg_temp.rejects($s$INSERT INTO transactions VALUES (102,1,1,101,'AAPL','Buy',2,100,1,'2026-01-01')$s$,'23503');
SELECT pg_temp.rejects($s$INSERT INTO transactions VALUES (101,3,1,101,'AAPL','Buy',2,100,0,'2026-01-01')$s$,'23505');
SELECT pg_temp.rejects($s$UPDATE transactions SET fees=1$s$,'23514');
SELECT pg_temp.rejects($s$DELETE FROM transactions$s$,'23514');
SELECT pg_temp.rejects($s$TRUNCATE transactions CASCADE$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE executions SET price=1$s$,'23514');
SELECT pg_temp.rejects($s$DELETE FROM executions$s$,'23514');
SELECT pg_temp.rejects($s$TRUNCATE executions CASCADE$s$,'23514');
SELECT pg_temp.rejects($s$DELETE FROM assets WHERE asset_id=101$s$,'23503');
SELECT pg_temp.rejects($s$UPDATE positions SET quantity=-1$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE positions SET average_cost=NULL$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE positions SET quantity=0$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE positions SET ledger_sequence=999$s$,'23503');
SELECT pg_temp.rejects($s$UPDATE portfolio_snapshots SET ledger_sequence=2 WHERE portfolio_id=102$s$,'23503');
SELECT pg_temp.rejects($s$UPDATE portfolio_snapshots SET cash=-1$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE portfolio_snapshots SET total_value=0$s$,'428C9');
SELECT pg_temp.rejects($s$UPDATE benchmark_snapshots SET symbol='AAPL'$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE benchmark_snapshots SET asset_id=101$s$,'23503');
SELECT pg_temp.rejects($s$UPDATE benchmark_snapshots SET as_of='2026-01-02'$s$,'23503');
SELECT pg_temp.rejects($s$UPDATE benchmark_snapshots SET range_start='2026-01-02'$s$,'23514');

DO $$
DECLARE cash numeric; shares numeric; ordering bigint[];
BEGIN
    SELECT 1000 + sum(CASE side WHEN 'Buy' THEN -quantity*price-fees ELSE quantity*price-fees END),
           sum(CASE side WHEN 'Buy' THEN quantity ELSE -quantity END),array_agg(order_id ORDER BY replay_sequence)
      INTO cash,shares,ordering FROM transactions WHERE portfolio_id=101;
    IF cash<>920 OR shares<>1 OR ordering<>ARRAY[1,2]::bigint[] OR
       (SELECT total_value FROM portfolio_snapshots WHERE portfolio_id=101)<>1040 THEN
        RAISE EXCEPTION 'Known ledger example or replay ordering failed';
    END IF;
    IF '0.123456789123456789'::positive_amount <> 0.123456789123456789::numeric THEN
        RAISE EXCEPTION 'Decimal precision lost';
    END IF;
END;
$$;
-- A closed projection retains realized P&L and has no average cost.
UPDATE positions SET quantity=0,average_cost=NULL,realized_pnl=-10;
-- Daily metadata is opt-in for legacy snapshots; partial or invalid metadata rejects.
SELECT pg_temp.rejects($s$UPDATE portfolio_snapshots SET session_date='2026-01-01' WHERE portfolio_id=101$s$,'23514');
UPDATE portfolio_snapshots SET session_date='2026-01-01',price_source='synthetic',cumulative_return=0.04
    WHERE portfolio_id=101;
SELECT pg_temp.rejects($s$UPDATE portfolio_snapshots SET daily_return=0 WHERE portfolio_id=101$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE portfolio_snapshots SET cumulative_return=-1.01 WHERE portfolio_id=101$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE portfolio_snapshots SET cumulative_return='NaN' WHERE portfolio_id=101$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE portfolio_snapshots SET price_source=' ' WHERE portfolio_id=101$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE portfolio_snapshots SET session_date='2026-01-02' WHERE portfolio_id=101$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE portfolio_snapshots SET previous_as_of=as_of WHERE portfolio_id=101$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE portfolio_snapshots SET previous_as_of='2025-12-31' WHERE portfolio_id=101$s$,'23503');
SELECT pg_temp.rejects($s$INSERT INTO portfolio_snapshots(portfolio_id,as_of,cash,market_value,session_date,price_source)
    VALUES(101,'2026-01-01 20:00Z',1000,0,'2026-01-01','synthetic')$s$,'23505');
SELECT pg_temp.rejects($s$UPDATE daily_valuation_marks SET close=0$s$,'23514');
SELECT pg_temp.rejects($s$UPDATE daily_valuation_marks SET symbol='UNKNOWN'$s$,'23503');
ROLLBACK;
\echo 'Schema constraint tests passed; fixtures rolled back'
