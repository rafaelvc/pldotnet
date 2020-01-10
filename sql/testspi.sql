CREATE OR REPLACE FUNCTION returnSelect2() RETURNS integer AS $$
return PlDotNet.CExecute("Select false", 1);
$$ LANGUAGE plcsharp;
SELECT returnSelect2();
