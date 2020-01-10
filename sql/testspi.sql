CREATE OR REPLACE FUNCTION returnSelect2() RETURNS integer AS $$
return PlDotNet.CExecute("SELECT 1.2", 1);
$$ LANGUAGE plcsharp;
SELECT returnSelect2();
