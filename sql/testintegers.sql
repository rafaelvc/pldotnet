CREATE OR REPLACE FUNCTION maxSmallInt() RETURNS smallint AS $$
return (short)32767;
$$ LANGUAGE plcsharp;
SELECT maxSmallInt();

CREATE OR REPLACE FUNCTION sum2SmallInt(a smallint, b smallint) RETURNS smallint AS $$
return (short)(a+b); //C# requires short cast
$$ LANGUAGE plcsharp;
SELECT sum2SmallInt(CAST(100 AS smallint), CAST(101 AS smallint));

CREATE OR REPLACE FUNCTION maxInteger() RETURNS integer AS $$
return 2147483647;
$$ LANGUAGE plcsharp;
SELECT maxInteger();

CREATE OR REPLACE FUNCTION sum2Integer(a integer, b integer) RETURNS integer AS $$
return a+b;
$$ LANGUAGE plcsharp;
SELECT sum2Integer(32770, 100);

CREATE OR REPLACE FUNCTION maxBigInt() RETURNS bigint AS $$
return 9223372036854775807;
$$ LANGUAGE plcsharp;
SELECT maxBigInt();

CREATE OR REPLACE FUNCTION sum2BigInt(a bigint, b bigint) RETURNS bigint AS $$
return a+b;
$$ LANGUAGE plcsharp;
SELECT sum2BigInt(9223372036854775707, 100);

CREATE OR REPLACE FUNCTION mixedBigInt(a integer, b integer, c bigint) RETURNS bigint AS $$
return (long)a+(long)b+c;
$$ LANGUAGE plcsharp;
SELECT mixedBigInt(32767,  2147483647, 100);

CREATE OR REPLACE FUNCTION mixedInt(a smallint, b smallint, c integer) RETURNS integer AS $$
return (int)a+(int)b+c;
$$ LANGUAGE plcsharp;
SELECT mixedInt(CAST(32767 AS smallint),  CAST(32767 AS smallint), 100);

-- FIX: the results are incorrect when mixing smallints with bigint 
--CREATE OR REPLACE FUNCTION mixedBigInt8(b smallint, c bigint) RETURNS smallint AS $$
--return (short)(b+c);
--$$ LANGUAGE plcsharp;
--SELECT mixedBigInt8(CAST(32 AS SMALLINT), CAST(100 AS BIGINT));

