--CREATE OR REPLACE FUNCTION returnX() RETURNS integer AS $$
--return 10;
--$$ LANGUAGE pldotnet;
--SELECT returnX();


--CREATE OR REPLACE FUNCTION returnSum() RETURNS integer AS $$
--return a+b;
--$$ LANGUAGE pldotnet;
--SELECT returnSum();


CREATE OR REPLACE FUNCTION inc2(val integer) RETURNS integer AS $$
return val + 1;
$$
LANGUAGE pldotnet;
SELECT inc2(8);


-- CREATE OR REPLACE FUNCTION returnSUM(a integer, b integer) RETURNS integer AS $$
-- return a + b;
-- $$
-- LANGUAGE pldotnet;
-- SELECT returnSUM(2,3);
