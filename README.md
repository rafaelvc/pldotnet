# PL/.NET

## Introduction

 PL/.NET a.k.a (***pldotnet***) adds Microsoft's .NET framework to PostgreSQL by 
introducing both the C# and F# as loadable procedural language.  

 Using PL/.NET developers can enjoy all benefits from .NET and its powerful 
application development environment to write code near the database using cool 
languages like C#. We at [Brick Abode](http://www.brickabode.com) love 
strongly-typed functional programming and as so we are also working to support 
F# for writting PostgreSQL functions and triggers in ***pldotnet*** 
(check [Future Plans](#future_plans) and [F# sample](#fsharp_sample)).

PL/.NET is an open source project developed by 
[Brick Abode](http://www.brickabode.com). You are welcome!  

## Installation

### Requirements

+ [PostgreSQL 9](https://www.postgresql.org/) or greater
+ [.NET Core 3.1](https://github.com/dotnet/core) or greater.
+ [Docker](https://www.docker.com/) (Optional and only for non Linux OSes)

### <a name="install_dotnetcore"></a>Installing .NET Core

For installing .NET Core please follow the instructions at the .NET Core 
[website](https://docs.microsoft.com/dotnet/core/install/linux-package-manager-ubuntu-1904).
  
There is a button in the header of that page where you select the version of 
Debian/Ubuntu distribution.  

Typically for Ubuntu 19.04 those are the steps:

    $ wget -q https://packages.microsoft.com/config/ubuntu/19.04/packages-microsoft-prod.deb -O packages-microsoft-prod.deb
    $ sudo dpkg -i packages-microsoft-prod.deb
    $ sudo apt-get update
    $ sudo apt-get install apt-transport-https
    $ sudo apt-get update
    $ sudo apt install dotnet-runtime-3.1 dotnet-sdk-3.1 dotnet-hostfxr-3.1

### Installing PL/.NET in Linux - Ubuntu/Debian flavours

Installing PostgreSQL:

    $ sudo apt install postgresql postgresql-common

Download the `pldotnet` binary package available 
[here](https://brickabode.com/pldtonet/postgres-10-pldotnet_0.1-1_amd64.deb) and 
install it: 

    $ sudo dpkg -i postgres-10-pldotnet_0.1-1_amd64.deb

The PL/.NET extension will be installed automatically and enabled
 by the `postgres` admin user for both C# (`plcsharp`) and F# (`plfsharp`).  

Be aware! In the end of the installation you are asked to turn `plcsharp` and 
`plfsharp` trusted PL languages. You need to do this for using PL/.NET on the 
current Alpha version. We do not consider PL/.NET ready for production but yet 
experimentable.
  
Case you need later turn PL/.NET languages trusted/untrusted pass `true` or 
`false` for the `pldtonet_trust` SQL function:

```sql
    postgres=# SELECT pldtonet_trust(true);
```

### <a name="other_oses"></a>Installing PL/.NET on other OSes

Currently we are developing for Linux Ubuntu 18.04 LTS however 
we plan to support all OSes are also supported by Postgres and .NET and you
have an option to experiment PL/.NET: Docker.
  
We constantly use Docker in our development computers then you also 
can benefit of it case your OS is not the supported one: 

    $ git clone https://github.com/brickabode/pldotnet.git
    $ docker-compose run pldotnet-devenv bash
    # make && make plnet-install
    # su postgres
    $ psql -c "CREATE EXTENSION pldotnet;"


###  Building Debian/Ubuntu package

Check [.NET Core](#install_dotnetcore) installation session first. All .NET 
requirements must be installed.

    $ git clone https://github.com/brickabode/pldotnet.git
    $ sudo apt install devscripts debhelper pkg-config postgresql-server-dev-all
    $ pg_buildext updatecontrol
    $ debuild -b -uc -us

`pg_buildext` is a script utility to build extensions for different PostgresSQL 
versions. You need to run `pg_buildext updatecontrol` in order to update the 
debian control according to the PG version installed in your Linux box.

#### Using different versions of PosgreSQL on Debian/Ubuntu

[PostgreSQL project](https://www.postgresql.org/download/) keeps packages 
repositories for different PG versions usually from 9x to newest versions and 
also for different Linux distributions flavours. You can follow steps 
[there](https://www.postgresql.org/download/) if you want to install others
PG versions. *Remember to run `pg_buildext updatecontrol` before building the 
`pldotnet` package for a newer PostgreSQL version.* 

## Examples

Many samples can be checked in our 
[test folder](https://github.com/brickabode/pldotnet/tree/master/sql). Some of 
them:

### C# (plcsharp)

Fibonnaci:
```fsharp
  CREATE FUNCTION fibbb(n integer) RETURNS integer AS $$
      int? ret = 1; // C# code
      if (n == 1 || n == 2) 
          return ret;
      return fibbb(n.GetValueOrDefault()-1) + fibbb(n.GetValueOrDefault()-2);;
  $$ LANGUAGE plcsharp;
```
Function call and output:
```sql
  # SELECT fibbb(30);
   fibbb
  --------
   832040
  (1 row)
```
  
Input and Output of Text 1:
```fsharp
  CREATE FUNCTION retVarCharText(fname varchar, lname varchar) RETURNS text AS $$
      return "Hello " + fname + lname + "!"; // C# code
  $$ LANGUAGE plcsharp;
```
Function call and output:
```sql
  CREATE FUNCTION
  # SELECT retVarCharText('Homer Jay ', 'Simpson');
        retvarchartext
  --------------------------
   Hello Homer Jay Simpson!
  (1 row)
```
  
Input and Output of Text 2:
```fsharp
  CREATE FUNCTION ageTest(name varchar, age integer, lname varchar) RETURNS varchar AS $$
      FormattableString res; // C# code
      if (age < 18)
          res = $"Hey {name} {lname}! Dude you are still a kid.";
      else if (age >= 18 && age < 40)
          res = $"Hey {name} {lname}! You are in the mood!";
      else
          res = $"Hey {name} {lname}! You are getting experienced!";
      return res.ToString();
  $$ LANGUAGE plcsharp;
```
Function call and output:
```sql
  # SELECT ageTest('Billy', 10, 'The KID') = varchar 'Hey Billy The KID! Dude you are still a kid.';
                     agetest
  ----------------------------------------------
   Hey Billy The KID! Dude you are still a kid.
  (1 row)
  
  # SELECT ageTest('John', 33, 'Smith') =  varchar 'Hey John Smith! You are in the mood!';
                 agetest
  --------------------------------------
   Hey John Smith! You are in the mood!
  (1 row)
  
  # SELECT ageTest('Robson', 41, 'Cruzoe') =  varchar 'Hey Robson Cruzoe! You are getting experienced!';
                       agetest
  -------------------------------------------------
   Hey Robson Cruzoe! You are getting experienced!
  (1 row)
```

### <a name="fsharp_sample"></a>F# (plfsharp)

Integer return:
```fsharp
  CREATE FUNCTION returnInt() RETURNS integer AS $$
      10 // F# code
  $$ LANGUAGE plfsharp;
```
Function call and output:
```sql
  # SELECT returnInt();
   returnint
  -----------
          10
  (1 row)
```

## Types support

PL/.NET uses different approaches for type conversion of function return and 
arguments for data exchange between PostgreSQL and .NET:

1. .NET [Bittable](https://docs.microsoft.com/en-us/dotnet/framework/interop/blittable-and-non-blittable-types) types are converted in its binary format.
2. PG text types are converted back and forth as [UTF8](https://pt.wikipedia.org/wiki/UTF-8) strings for best compatibility.
3. Numeric PG type is converted back and forth as [C String](https://en.wikipedia.org/wiki/C_string_handling).
4. For null support, types are converted to its respective .NET `Nullable<>` 
type (`bool`, `integers`).

The following table shows each type conversion equivalences:

| PostgreSQL type     | C# type                            | F# type               |
|---------------------|------------------------------------|-----------------------|
| boolean             | Nullable<System.Boolean> (`bool?`) | < Not yet supported > |
| smallint (int2)     | Nullable<System.Int16> (`short?`)  | < Not yet supported > |
| integer  (int4)     | Nullable<System.Int32> (`int?`)    | System.Int32 (`int`)  |
| bigint   (int8)     | Nullable<System.Int64> (`long?`)   | < Not yet supported > |
| real     (float4)   | System.Single (`float`)            | < Not yet supported > |
| double   (float8)   | System.Double (`double`)           | < Not yet supported > |
| char, varchar, text | System.String (`string`)           | < Not yet supported > |
| "char"/bpchar       | System.String (`string`)           | < Not yet supported > |
| numeric             | System.Decimal (`decimal`)         | < Not yet supported > |
| Arrays              | (Planned for Beta Release)         | < Not yet supported > |
| Composite           | (Planned for Beta Release)         | < Not yet supported > |
| Base, domain        | (Planned for 1.0 Release)          | < Not yet supported > |


## <a name="future_plans"></a>Future Plans
  - Arrays
    + Add array support for plcsharp. Beta version.
  - Composites
    + Add composites support for plcsharp. Beta version.
  - SPI
    + Add support to the PostgeSQL SPI (Server Programming API) enabling 
query/plans and database access. Beta version.
  - Triggers
    + Add trigger writing support in plcsharp. Beta version.
  - F#
    + Basic data types
      * Expand F# basic types support. Beta version.
    + F# Compiler Services
      * Add [F# Compiler Services](http://fsharp.github.io/FSharp.Compiler.Service/) 
API for performance improvement regarding source code compilation.
  - Additional data types
    + Add other data types support for F# and C#, like:
      * `binary`
      * `timezone`
      * `json`
      * `range`
      * `geometry`
      * `internet`
      * `bitstring`

## License

PL/.NET uses [Apache 2.0](https://www.apache.org/licenses/LICENSE-2.0) license.
