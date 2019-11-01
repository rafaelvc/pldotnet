CREATE OR REPLACE FUNCTION retVarChar(fname varchar) RETURNS varchar AS $$
return fname + " Cabral";
$$ LANGUAGE pldotnet;
SELECT retVarChar('Rafael');

CREATE OR REPLACE FUNCTION retConcatVarChar(fname varchar, lname varchar) RETURNS varchar AS $$
return fname + lname;
$$ LANGUAGE pldotnet;
SELECT retConcatVarChar('João ', 'da Silva');
