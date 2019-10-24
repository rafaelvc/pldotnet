-- Float4 (real): 6 digits of precison
CREATE OR REPLACE FUNCTION returnBool() RETURNS boolean AS $$
return false;
$$ LANGUAGE pldotnet;
SELECT returnBool();

