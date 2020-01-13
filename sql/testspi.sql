CREATE OR REPLACE FUNCTION returnSelect2() RETURNS integer AS $$
return PlDotNet.CExecute("SELECT CAST(1.2 as real)", 1);
$$ LANGUAGE plcsharp;
SELECT returnSelect2();
