CREATE OR REPLACE FUNCTION returnSelect2() RETURNS double precision AS $$
var exp = PlDotNet.SPIExecute("SELECT CAST(11.0050000000005 as double precision) as column", 1);
Console.WriteLine(exp.Count);
return 1.2;
$$ LANGUAGE plcsharp;
SELECT returnSelect2();
