CREATE OR REPLACE FUNCTION returnSelect2() RETURNS double precision AS $$
var exp = PlDotNet.SPIExecute("SELECT 1 as c, 2 as b", 1);
Console.WriteLine("start");
foreach (var row in exp) {
    Console.WriteLine(row.b);
    Console.WriteLine(row.c);
}
Console.WriteLine("end");
return 1.2;
$$ LANGUAGE plcsharp;
SELECT returnSelect2();
