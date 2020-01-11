
CREATE TYPE PersonG AS (
    name            text,
    idade           integer,
    peso            double precision,
    altura          real,
    salario         double precision,
    casado          boolean
);

CREATE OR REPLACE FUNCTION hellonew7(a PersonG) RETURNS integer AS $$
int n = 1;
FormattableString res;
a.idade += n;
res = $"Hello M(r)(s). {a.name}! Your age {a.idade}, {a.peso}, {a.altura}, {a.salario}. {a.casado}";
Console.WriteLine(res.ToString());
return a.idade;
$$ LANGUAGE plcsharp;
SELECT hellonew7(('Rafael da Veiga Cabral', 38, 85.5, 1.71, 10.5555, true));

--CREATE OR REPLACE FUNCTION hello_aaa(a NameAge) RETURNS text AS $$
--BEGIN
--    RETURN a.name;
--END;
--$$ LANGUAGE plpgsql;
--SELECT hello_aaa(('Rafael Cabral', 38));



