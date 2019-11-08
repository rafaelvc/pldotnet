-- Test this only to check that PLSQL seems not to be 
-- caring of the precision/scale:
--CREATE OR REPLACE FUNCTION get_sum(
--   a NUMERIC(3,2), 
--   b NUMERIC) 
--RETURNS NUMERIC(4,1) AS $$
--BEGIN
--   RETURN a + b;
--END; $$
--LANGUAGE plpgsql;
--
--SELECT get_sum(2.3333333, 10);

CREATE OR REPLACE FUNCTION get_sum(
   a NUMERIC(3,2), 
   b NUMERIC) 
RETURNS NUMERIC(4,1) AS $$
return a + b;
$$
LANGUAGE pldotnet;
SELECT get_sum(1.3333333, 10);
SELECT get_sum(1.33333333, -10.99999999);
SELECT get_sum(1999999999999.555555555555555, -10.99999999); -- 1999999999988.555555565555555

CREATE OR REPLACE FUNCTION getbigNum(a NUMERIC) RETURNS NUMERIC AS $$
return a;
$$
LANGUAGE pldotnet;
SELECT getbigNum(999999999999999999991.9999991); -- Sextllion at 7 scale (10 power 28 precision)
SELECT getbigNum(999999999999999999991.99999999); -- It is rounded to 999999999999999999992.0000000



