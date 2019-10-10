CREATE OR REPLACE FUNCTION returnX() RETURNS integer AS $$
return 10;
$$ LANGUAGE pldotnet;
SELECT returnX();
