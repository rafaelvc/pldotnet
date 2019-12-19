#+TITLE: PL/.NET

* Introduction
 PL/.NET adds Microsoft's .NET framework to PostgreSQL by introducing both the C# and F#
 languages as loadable procedural language. Using PL/.NET one can enjoy all benefits from
 .NET and its powerful application development environment and also from a strongly-typed
 functional programming language, F#, when writing functions and triggers (check Future Plans)
 written in those languages. 
* Installation

PL/.NET is installed like any regular PostgreSQL extension. It requires [[ ][.NET Core 3.0]] or later.
First, obtain it by cloning the repository, after that just run:

#+BEGIN_SRC shell
$ make && make plnet-install
$ psql -c "CREATE EXTENSION pldotnet;" <mydb>
#+END_SRC

The PL/.NET extension installs both C# (~plcsharp~) and F# (~plfsharp~) modules
for using them as loadable procedure languages.

* Types

PL/.NET uses three different approaches for type conversion of function's
arguments between PostgreSQL and .NET runtime environment, here's a list of them:

1. Convert to string/text (~decimal~);
2. Natively convert using ~System.Runtime.InteropServices~;
3. For null support, convert the type to its respective ~Nullable<>~ type (~bool~, ~integers~).

The following table shows each type conversion equivalences:

| PostgreSQL type     | C# type                            | F# type               |
|---------------------+------------------------------------+-----------------------|
| bool                | Nullable<System.Boolean> (~bool?~) | < Not yet supported > |
| int2                | Nullable<System.Int16> (~short?~)  | < Not yet supported > |
| int4                | Nullable<System.Int32> (~int?~)    | System.Int32 (~int~)  |
| int8                | Nullable<System.Int64> (~long?~)   | < Not yet supported > |
| float4              | System.Single (~float~)            | < Not yet supported > |
| float8              | System.Double (~double~)           | < Not yet supported > |
| char, varchar, text | System.String (~string~)           | < Not yet supported > |
| "char"/bpchar       | System.String (~string~)           | < Not yet supported > |
| numeric             | System.Decimal (~decimal~)         | < Not yet supported > |
| Arrays              | < Not yet supported >              | < Not yet supported > |
| Composite           | < Not yet supported >              | < Not yet supported > |
| Base, domain        | < Not yet supported >              | < Not yet supported > |
* Functions
  Functions PG/.NET languages are created as:

#+BEGIN_SRC sql
CREATE FUNCTION func(args) RETURN return_type AS $$
    -- < C# / F# function body >
$$ LANGUAGE [ plcsharp | plfsharp];
#+END_SRC

where the types of the named arguments (~args~) and ~return_type~ are converted
following the table in Types section.

The function body are composed to its respectively language chunk following the
templates below:

+ C#

#+BEGIN_SRC csharp
libargs.resu = FUNC(args);
return_type FUNC(args) {
   // C# function body
}
#+END_SRC

+ F#

#+BEGIN_SRC fsharp
type Lib =
    static member FUNC =
        // F# function body
    static member Main =
        libargs.resu <- FUNC args
#+END_SRC

PL/.NET hosts the .NET runtime using ~hostfxr~ for loading the delegates, that's why
the procedure function body are inserted into a class structure for both C# and F#.

** Examples
   + C#

#+BEGIN_SRC sql
# CREATE FUNCTION retVarCharText(fname varchar, lname varchar) RETURNS text AS $$
return "Hello " + fname + lname + "!";
$$ LANGUAGE plcsharp;
CREATE FUNCTION
# SELECT retVarCharText('Homer Jay ', 'Simpson');
      retvarchartext
--------------------------
 Hello Homer Jay Simpson!
(1 row)

#+END_SRC

#+BEGIN_SRC sql
# CREATE FUNCTION ageTest(name varchar, age integer, lname varchar) RETURNS varchar AS $$
FormattableString res;
if (age < 18)
    res = $"Hey {name} {lname}! Dude you are still a kid.";
else if (age >= 18 && age < 40)
    res = $"Hey {name} {lname}! You are in the mood!";
else
    res = $"Hey {name} {lname}! You are getting experienced!";
return res.ToString();
$$ LANGUAGE plcsharp;
CREATE FUNCTION
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

#+END_SRC

#+BEGIN_SRC sql
# CREATE FUNCTION fibbb(n integer) RETURNS integer AS $$
    int? ret = 1;
    if (n == 1 || n == 2) 
        return ret;
    return fibbb(n.GetValueOrDefault()-1) + fibbb(n.GetValueOrDefault()-2);;
$$ LANGUAGE plcsharp;
CREATE FUNCTION
# SELECT fibbb(30);
 fibbb
--------
 832040
(1 row)

#+END_SRC

   + F#

#+BEGIN_SRC sql
# CREATE FUNCTION returnInt() RETURNS integer AS $$
10
$$ LANGUAGE plfsharp;
CREATE FUNCTION
# SELECT returnInt();
 returnint
-----------
        10
(1 row)

#+END_SRC

* Future Plans
  - Arrays
    + Add array support for both C# and F# languages. Beta version.
  - Composites
    + Add composites support for both C# and F# languages. Beta version.
  - SPI
    + Add SPI usage support for both C# and F#. Beta version.
  - Triggers
    + Add trigger writing support in both C# and F# languages. Beta version.
  - F#
    + Basic data types
      * Expand F# supported basic types. Beta version.
    + F# Compiler Services
      * Add [[http://fsharp.github.io/FSharp.Compiler.Service/][F# Compiler Services]] API for performance improvement regarding source code compilation.
  - Additional data types
    + Add other data types support for F# and C#, like:
      * ~binary~
      * ~timezone~
      * ~json~
      * ~range~
      * ~geometry~
      * ~internet~
      * ~bitstring~
* License
