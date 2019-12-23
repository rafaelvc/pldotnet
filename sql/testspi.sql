CREATE OR REPLACE FUNCTION returnSelect2() RETURNS integer AS $$
return PlDotNet.SPIExecute("Select 2", 1);
$$ LANGUAGE plcsharp;
SELECT returnSelect2();
