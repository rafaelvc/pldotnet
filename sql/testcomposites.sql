
CREATE TYPE NameAge AS (
    name            text,
    age             integer
);

CREATE OR REPLACE FUNCTION hello(a name_age) RETURNS text AS $$
int n = 1;
FormattableString res;
a.age += n;
res = $"Hello M(r)(s). ${a.name}! Your age plus ${n} is equal to ${a.age}."
return res.ToString();
$$ LANGUAGE plcsharp;
SELECT hello(('Rafael Cabral', 38));

--CREATE OR REPLACE FUNCTION hello_aaa(a NameAge) RETURNS text AS $$
--BEGIN
--    RETURN a.name;
--END;
--$$ LANGUAGE plpgsql;
--SELECT hello_aaa(('Rafael Cabral', 38));



