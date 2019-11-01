CREATE OR REPLACE FUNCTION retVarChar(fname varchar) RETURNS varchar AS $$
return fname + " Cabral";
$$ LANGUAGE pldotnet;
SELECT retVarChar('Rafael');

CREATE OR REPLACE FUNCTION retConcatVarChar(fname varchar, lname varchar) RETURNS varchar AS $$
return fname + lname;
$$ LANGUAGE pldotnet;
SELECT retConcatVarChar('João ', 'da Silva');

CREATE OR REPLACE FUNCTION retConcatText(fname text, lname text) RETURNS text AS $$
return "Hello " + fname + lname + "!";
$$ LANGUAGE pldotnet;
SELECT retConcatText('João ', 'da Silva');

CREATE OR REPLACE FUNCTION retVarCharText(fname varchar, lname varchar) RETURNS text AS $$
return "Hello " + fname + lname + "!";
$$ LANGUAGE pldotnet;
SELECT retVarCharText('Homer Jay ', 'Simpson');

CREATE OR REPLACE FUNCTION retChar(argchar character) RETURNS character AS $$
return argchar;
$$ LANGUAGE pldotnet;
SELECT retChar('R');

--CREATE OR REPLACE FUNCTION retConcatLeter(fletter character, letter character) RETURNS varchar AS $$
--return ";
--$$ LANGUAGE pldotnet;
--SELECT retTextVarChar('Lisa ', 'Simpson');
