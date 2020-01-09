
CREATE TYPE NameAgeC AS (
    name            integer,
    age             integer
);

CREATE OR REPLACE FUNCTION hellonew2(a NameAgeC) RETURNS text AS $$
int n = 1;
FormattableString res;
a.age += n;
res = $"Hello M(r)(s). ${a.name}! Your age plus ${n} is equal to ${a.age}.";
return res.ToString();
$$ LANGUAGE plcsharp;
SELECT hellonew2((38, 38));

--CREATE OR REPLACE FUNCTION hello_aaa(a NameAge) RETURNS text AS $$
--BEGIN
--    RETURN a.name;
--END;
--$$ LANGUAGE plpgsql;
--SELECT hello_aaa(('Rafael Cabral', 38));



