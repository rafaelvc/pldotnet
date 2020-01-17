CREATE TYPE Person AS (
    name            text,
    age             integer,
    weight          double precision,
    height          real,
    salary          double precision,
    married         boolean
);

CREATE OR REPLACE FUNCTION helloPersonAge(per Person) RETURNS integer AS $$
    int n = 1;
    FormattableString res;
    res = $"Hello M(r)(s). {per.name}! Your age {per.age}, {per.weight}, {per.height}, {per.salary}. {per.married}";
    Console.WriteLine(res.ToString());
    return per.age;
$$ LANGUAGE plcsharp;
SELECT helloPersonAge(('John Smith', 38, 85.5, 1.71, 999.999, true));

CREATE OR REPLACE FUNCTION helloPerson(p Person) RETURNS Person AS $$
    int n = 1;
    FormattableString res;
    p.age += n;
    res = $"Hello M(r)(s). {p.name}! Your age {p.age}, {p.weight}, {p.height}, {p.salary}. {p.married}";
    Console.WriteLine(res.ToString());
    return p;
$$ LANGUAGE plcsharp;
SELECT helloPerson(('John Smith', 38, 85.5, 1.71, 999.999, true));
