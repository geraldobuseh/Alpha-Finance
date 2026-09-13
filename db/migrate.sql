\set ON_ERROR_STOP on
BEGIN;
SET LOCAL search_path = public, pg_catalog;
-- Serialize migration runners; history and DDL commit together.
SELECT pg_advisory_xact_lock(710010);
CREATE TABLE IF NOT EXISTS public.schema_migrations (
    version integer PRIMARY KEY CHECK (version > 0),
    description text NOT NULL CHECK (btrim(description) <> ''),
    applied_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP CHECK (isfinite(applied_at))
);
SELECT EXISTS (SELECT 1 FROM public.schema_migrations WHERE version = 1) AS applied \gset
\if :applied
    \echo 'Migration 001 already applied'
\else
    \ir migrations/001_v01_schema.sql
    INSERT INTO public.schema_migrations(version, description)
    VALUES (1, 'PQL-010 v0.1 financial schema');
\endif
SELECT EXISTS (SELECT 1 FROM public.schema_migrations WHERE version = 2) AS applied \gset
\if :applied
    \echo 'Migration 002 already applied'
\else
    \ir migrations/002_daily_market_prices.sql
    INSERT INTO public.schema_migrations(version, description)
    VALUES (2, 'PQL-014 daily market-data session identity');
\endif
SELECT EXISTS (SELECT 1 FROM public.schema_migrations WHERE version = 3) AS applied \gset
\if :applied
    \echo 'Migration 003 already applied'
\else
    \ir migrations/003_daily_valuations.sql
    INSERT INTO public.schema_migrations(version, description)
    VALUES (3, 'PQL-024 daily valuation returns and closing-price provenance');
\endif
COMMIT;
