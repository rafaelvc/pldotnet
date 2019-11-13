/**************** Null return test functions ***************************/
CREATE OR REPLACE FUNCTION returnNullInt() RETURNS integer AS $$
return null;
$$ LANGUAGE pldotnet;
SELECT returnNullInt();

CREATE OR REPLACE FUNCTION returnNullSmallInt() RETURNS smallint AS $$
return null;
$$ LANGUAGE pldotnet;
SELECT returnNullSmallInt();

CREATE OR REPLACE FUNCTION returnNullBigInt() RETURNS bigint AS $$
return null;
$$ LANGUAGE pldotnet;
SELECT returnNullBigInt();

/**************** Null operations test functions ***************************/
CREATE OR REPLACE FUNCTION sumNullArgInt(a integer, b integer) RETURNS integer AS $$
return a + b;
$$
LANGUAGE pldotnet;
SELECT sumNullArgInt(null,null);
SELECT sumNullArgInt(null,3);
SELECT sumNullArgInt(3,null);
SELECT sumNullArgInt(3,3);

CREATE OR REPLACE FUNCTION sumNullArgSmallInt(a smallint, b smallint) RETURNS smallint AS $$
return (short?)(a + b);
$$
LANGUAGE pldotnet;
SELECT sumNullArgSmallInt(null,null);
SELECT sumNullArgSmallInt(null,CAST(101 AS smallint));
SELECT sumNullArgSmallInt(CAST(101 AS smallint),null);
SELECT sumNullArgSmallInt(CAST(101 AS smallint),CAST(101 AS smallint));

CREATE OR REPLACE FUNCTION sumNullArgBigInt(a bigint, b bigint) RETURNS bigint AS $$
return a + b;
$$
LANGUAGE pldotnet;
SELECT sumNullArgBigInt(null,null);
SELECT sumNullArgBigInt(null,100);
SELECT sumNullArgBigInt(92233720368547757707,null);
SELECT sumNullArgBigInt(9223372036854775707,100);

/**************** Conditional return test functions ***************************/
CREATE OR REPLACE FUNCTION checkedSumNullArgInt(a integer, b integer) RETURNS integer AS $$
if(!a.HasValue || !b.HasValue)
    return null;
else
    return a + b;
$$
LANGUAGE pldotnet;
SELECT checkedSumNullArgInt(null,null);
SELECT checkedSumNullArgInt(null,3);
SELECT checkedSumNullArgInt(3,null);
SELECT checkedSumNullArgInt(3,3);

CREATE OR REPLACE FUNCTION checkedSumNullArgSmallInt(a smallint, b smallint) RETURNS smallint AS $$
if(!a.HasValue || !b.HasValue)
    return null;
else
    return (short?)(a + b);
$$
LANGUAGE pldotnet;
SELECT checkedSumNullArgSmallInt(null,null);
SELECT checkedSumNullArgSmallInt(null,CAST(133 AS smallint));
SELECT checkedSumNullArgSmallInt(CAST(133 AS smallint),null);
SELECT checkedSumNullArgSmallInt(CAST(133 AS smallint),CAST(133 AS smallint));

CREATE OR REPLACE FUNCTION checkedSumNullArgBigInt(a bigint, b bigint) RETURNS bigint AS $$
if(!a.HasValue || !b.HasValue)
    return null;
else
    return a + b;
$$
LANGUAGE pldotnet;
SELECT checkedSumNullArgBigInt(null,null);
SELECT checkedSumNullArgBigInt(null,100);
SELECT checkedSumNullArgBigInt(9223372036854775707,null);
SELECT checkedSumNullArgBigInt(9223372036854775707,100);

/**************** Conditional return test functions (Mixed Args)  ***************************/
CREATE OR REPLACE FUNCTION checkedSumNullArgMixed(a integer, b smallint, c bigint) RETURNS bigint AS $$
if(!a.HasValue || !b.HasValue || !c.HasValue)
    return null;
else
    return (long)a + (long)b + c;
$$
LANGUAGE pldotnet;
SELECT checkedSumNullArgMixed(null,null,null);
SELECT checkedSumNullArgMixed(null,3,null);
SELECT checkedSumNullArgMixed(3,null,null);
SELECT checkedSumNullArgMixed(null,null,3);
SELECT checkedSumNullArgMixed(1313,CAST(1313 as smallint), 1313);

