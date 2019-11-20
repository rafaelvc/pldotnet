CREATE OR REPLACE FUNCTION returnBool() RETURNS boolean AS $$
return false;
$$ LANGUAGE plcsharp;
SELECT returnBool();

CREATE OR REPLACE FUNCTION BooleanAnd(a boolean, b boolean) RETURNS boolean AS $$
return a&b;
$$ LANGUAGE plcsharp;
SELECT BooleanAnd(true, true);

CREATE OR REPLACE FUNCTION BooleanOr(a boolean, b boolean) RETURNS boolean AS $$
return a|b;
$$ LANGUAGE plcsharp;
SELECT BooleanOr(false, false);

CREATE OR REPLACE FUNCTION BooleanXor(a boolean, b boolean) RETURNS boolean AS $$
return a^b;
$$ LANGUAGE plcsharp;
SELECT BooleanXor(false, false);

