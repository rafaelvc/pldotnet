-- Float4 (real): 6 digits of precison
CREATE OR REPLACE FUNCTION returnReal() RETURNS real AS $$
return 1.50055f;
$$ LANGUAGE pldotnet;
SELECT returnReal();

CREATE OR REPLACE FUNCTION sumReal(a real, b real) RETURNS real AS $$
return a+b;
$$ LANGUAGE pldotnet;
SELECT sumReal(1.50055, 1.50054); -- 3.00109

--- Float8 (double precision): 15 digits of precison
CREATE OR REPLACE FUNCTION returnDouble() RETURNS double precision AS $$
return 11.0050000000005;
$$ LANGUAGE pldotnet;
SELECT returnDouble();

CREATE OR REPLACE FUNCTION sumDouble(a double precision, b double precision) RETURNS double precision AS $$
return a+b;
$$ LANGUAGE pldotnet;
SELECT sumDouble(10.5000000000055, 10.5000000000054); -- 21.0000000000109
