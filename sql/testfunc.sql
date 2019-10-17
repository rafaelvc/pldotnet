--CREATE OR REPLACE FUNCTION returnX() RETURNS integer AS $$
--return 2147483647;
--$$ LANGUAGE pldotnet;
--SELECT returnX();

--CREATE OR REPLACE FUNCTION returnSmallInt() RETURNS smallint AS $$
--return 32767;
--$$ LANGUAGE pldotnet;
--SELECT returnSmallInt();


CREATE OR REPLACE FUNCTION smallInt1(a smallint, b smallint) RETURNS smallint AS $$
return (short)(a+b);
$$ LANGUAGE pldotnet;
SELECT smallInt1(CAST(100 AS smallint), CAST(101 AS smallint));


--CREATE OR REPLACE FUNCTION retBIGInt5() RETURNS bigint AS $$
--return 9223372036854775707;
--$$ LANGUAGE pldotnet;
--SELECT retBIGInt5();

--CREATE OR REPLACE FUNCTION inc2(val integer) RETURNS integer AS $$
--return val + 2;
--$$
--LANGUAGE pldotnet;
--SELECT inc2(8);
--
--CREATE OR REPLACE FUNCTION sum2(a integer, b integer) RETURNS integer AS $$
--return a + b;
--$$
--LANGUAGE pldotnet;
--SELECT sum2(3,2);
--
--CREATE OR REPLACE FUNCTION sum3(aaa integer, bbb integer, ccc integer) RETURNS integer AS $$
--return aaa + bbb + ccc;
--$$
--LANGUAGE pldotnet;
--SELECT sum3(3,2,1);
--
--CREATE OR REPLACE FUNCTION sum4(a integer, b integer, c integer, d integer) RETURNS integer AS $$
--return a + b + c + d;
--$$
--LANGUAGE pldotnet;
--SELECT sum4(4,3,2,1);


