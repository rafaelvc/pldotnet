CREATE OR REPLACE FUNCTION returnNullBool() RETURNS boolean AS $$
return null;
$$ LANGUAGE pldotnet;
SELECT returnNullBool();

CREATE OR REPLACE FUNCTION BooleanNullAnd(a boolean, b boolean) RETURNS boolean AS $$
return a&b;
$$ LANGUAGE pldotnet;
SELECT BooleanNullAnd(true, null);
SELECT BooleanNullAnd(null, true);
SELECT BooleanNullAnd(false, null);
SELECT BooleanNullAnd(null, false);
SELECT BooleanNullAnd(null, null);

CREATE OR REPLACE FUNCTION BooleanNullOr(a boolean, b boolean) RETURNS boolean AS $$
return a|b;
$$ LANGUAGE pldotnet;
SELECT BooleanNullOr(true, null);
SELECT BooleanNullOr(null, true);
SELECT BooleanNullOr(false, null);
SELECT BooleanNullOr(null, false);
SELECT BooleanNullOr(null, null);

CREATE OR REPLACE FUNCTION BooleanNullXor(a boolean, b boolean) RETURNS boolean AS $$
return a^b;
$$ LANGUAGE pldotnet;
SELECT BooleanNullXor(true, null);
SELECT BooleanNullXor(null, true);
SELECT BooleanNullXor(false, null);
SELECT BooleanNullXor(null, false);
SELECT BooleanNullXor(null, null);
