
--CREATE TYPE NameAgeC AS (
--    name            integer,
--    age             integer
--);

CREATE OR REPLACE FUNCTION hellonew3(a NameAgeC) RETURNS integer AS $$
int n = 1;
FormattableString res;
a.age += n;
res = $"Hello M(r)(s). {a.name}! Your age plus {n} is equal to {a.age}.";
Console.WriteLine(res.ToString());
return 1;
$$ LANGUAGE plcsharp;
SELECT hellonew3((38, 38));

--CREATE OR REPLACE FUNCTION hello_aaa(a NameAge) RETURNS text AS $$
--BEGIN
--    RETURN a.name;
--END;
--$$ LANGUAGE plpgsql;
--SELECT hello_aaa(('Rafael Cabral', 38));



