-- This function won't work see #30
--CREATE OR REPLACE FUNCTION fibbb(n integer) RETURNS integer AS $$
--    if (n == 1)
--        return 1;
--    if (n == 2)
--        return 1;
--    return fibbb(n-1) + fibbb(n - 2);
--$$ LANGUAGE pldotnet;

-- Workarround for #30 and we need review performace for n > 40 (stackoverflow?)
CREATE OR REPLACE FUNCTION fibbb(n integer) RETURNS integer AS $$
    int? ret = 1;
    if (n == 1 || n == 2) 
        return ret;
    return fibbb(n.GetValueOrDefault()-1) + fibbb(n.GetValueOrDefault()-2);;
$$ LANGUAGE pldotnet;
SELECT fibbb(30);

CREATE OR REPLACE FUNCTION fact(n integer) RETURNS integer AS $$
    int? ret = 1;
    if (n <= 1) 
        return ret;
    else
    	return n*fact(n.GetValueOrDefault()-1);
$$ LANGUAGE pldotnet;
SELECT fact(5);

CREATE OR REPLACE FUNCTION natural(n numeric) RETURNS numeric AS $$
    if (n < 0) 
        return 0;
    else if (n == 1)
	return 1;
    else
    	return natural(n-1);
$$ LANGUAGE pldotnet;
SELECT natural(10);
SELECT natural(10.5);
