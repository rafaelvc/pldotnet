CREATE OR REPLACE FUNCTION sumName(fstnm varchar, lstnm varchar) RETURNS varchar AS $$
return fstnm + lstnm;
$$ LANGUAGE pldotnet;
SELECT sumName('Rafael', 'Cabral');


