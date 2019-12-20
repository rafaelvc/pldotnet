CREATE OR REPLACE FUNCTION returnBool() RETURNS boolean AS $$
PlDotNet.SPIExecute("Select 2", 1);
return false;
$$ LANGUAGE plcsharp;
SELECT returnBool();
