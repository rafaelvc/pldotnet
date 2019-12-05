CREATE OR REPLACE FUNCTION sumArray(a integer[]) RETURNS integer AS $$
return a[0] + a[1] + a[2];
$$ LANGUAGE pldotnet;
SELECT sumArray( ARRAY[4,1,5] ) = integer '10';

