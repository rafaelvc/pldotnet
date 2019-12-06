CREATE OR REPLACE FUNCTION sumArray(a integer[]) RETURNS integer AS $$
return a[0] + a[1] + a[2];
$$ LANGUAGE pldotnet;
SELECT sumArray( ARRAY[4,1,5] ) = integer '10';

CREATE OR REPLACE FUNCTION sumArray(a numeric[]) RETURNS numeric AS $$
return a[0] + a[1] + a[2];
$$ LANGUAGE pldotnet;
SELECT sumArray( ARRAY[1.00002, 1.00003, 1.00004] ) = numeric '3.00009';

CREATE OR REPLACE FUNCTION sumArray(a numeric) RETURNS numeric AS $$
return a+(decimal)2.5;
$$ LANGUAGE pldotnet;
SELECT sumArray( 1.00002 ) = numeric '3.50002';

CREATE OR REPLACE FUNCTION sumArray1(a text[]) RETURNS text AS $$
return a[0] + a[1] + a[2] + a[3];
$$ LANGUAGE pldotnet;
SELECT sumArray1( ARRAY['Rafael', ' da', ' Veiga', ' Cabral'] ) = varchar 'Rafael da Veiga Cabral';

