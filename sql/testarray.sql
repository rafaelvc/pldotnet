CREATE OR REPLACE FUNCTION sumArray(a integer[]) RETURNS integer AS $$
Console.WriteLine($"len: {a.Length}");
Console.WriteLine($"fst: {a[0]}");
return a[0] + a[1] + a[2];
$$ LANGUAGE pldotnet;
SELECT sumArray( ARRAY[4,1,5] );

