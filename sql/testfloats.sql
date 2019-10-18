-- Float: 6 digits of precison
CREATE OR REPLACE FUNCTION returnAFloat() RETURNS real AS $$
return 10.000005f;
$$ LANGUAGE pldotnet;
SELECT returnAFloat();

