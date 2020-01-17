CREATE OR REPLACE FUNCTION returnCompositeSum() RETURNS integer AS $$
var exp = PlDotNet.SPIExecute("SELECT 1 as c, 2 as b", 1);
int sum = 0;
foreach (var row in exp)
{
    sum += row.b + row.c;
}
return sum;
$$ LANGUAGE plcsharp;
SELECT returnCompositeSum() = integer '3';

CREATE TABLE pldotnettypes (
    bcol  BOOLEAN,
    i2col SMALLINT,
    i4col INTEGER,
    i8col BIGINT,
    f4col FLOAT,
    f8col DOUBLE PRECISION,
    ncol  NUMERIC,
    vccol VARCHAR
);
INSERT INTO pldotnettypes VALUES (
    true,
    CAST(1 as INT2),
    CAST(32767 as INT4),
    CAST(9223372036854775707 as BIGINT),
    CAST(1.4 as FLOAT),
    CAST(10.5000000000055 as DOUBLE PRECISION),
    CAST(1.2 as NUMERIC),
    'StringSample;'
);
CREATE OR REPLACE FUNCTION checkTypes() RETURNS boolean AS $$
var exp = PlDotNet.SPIExecute("SELECT * from pldotnettypes", 1);
foreach (var row in exp)
{
    if(
        row.bcol.GetType() != typeof(bool)
        && row.i2col.GetType() != typeof(short)
        && row.i4col.GetType() != typeof(int)
        && row.i8col.GetType() != typeof(long)
        && row.f4col.GetType() != typeof(float)
        && row.f8col.GetType() != typeof(double)
        && row.ncol.GetType() != typeof(decimal)
        && row.vccol.GetType() != typeof(string)
    )
    {
        return false;
    }
}
return true;
$$ LANGUAGE plcsharp;
SELECT checkTypes() is true;

CREATE TABLE usersavings(ssnum int8, name varchar, sname varchar, balance float4);
INSERT INTO usersavings VALUES (123456789,'Homer','Simpson',2304.55);
INSERT INTO usersavings VALUES (987654321,'Charles Montgomery','Burns',3000000.65);
CREATE OR REPLACE FUNCTION getUsersWithBalance(searchbalance real) RETURNS varchar AS $$
var exp = PlDotNet.SPIExecute("SELECT * from usersavings", 1);
string res = $"User(s) found with {searchbalance} account balance";
foreach (var user in exp)
{
    if(user.balance == searchbalance)
    {
       res += $", {user.name} {user.sname} (Social Security Number {user.ssnum})";
    }
}
res += ".";
return res;
$$ LANGUAGE plcsharp;
SELECT getUsersWithBalance(2304.55) = varchar 'User(s) found with 2304.55 account balance, Homer Simpson (Social Security Number 123456789).';

CREATE OR REPLACE FUNCTION getUserDescription(ssnum bigint) RETURNS varchar AS $$
var exp = PlDotNet.SPIExecute($"SELECT * from usersavings WHERE ssnum={ssnum}", 1);
string res = "No user found";
foreach (var user in exp)
{
    if(user.ssnum == ssnum)
    {
        res = $"{user.name} {user.sname}, Social security Number {user.ssnum}, has {user.balance} account balance.";
    }
}
return res;
$$ LANGUAGE plcsharp;
SELECT getUserDescription(123456789) = varchar 'Homer Simpson, Social security Number 123456789, has 2304.55 account balance.';
SELECT getUserDescription(987654321) = varchar 'Charles Montgomery Burns, Social security Number 987654321, has 3000000.8 account balance.';
