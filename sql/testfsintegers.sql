CREATE OR REPLACE FUNCTION sum2Integer(a integer, b integer) RETURNS integer AS $$
a+b
$$ LANGUAGE plfsharp;
SELECT sum2Integer(1000, 1) = integer '1001';

