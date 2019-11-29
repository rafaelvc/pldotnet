\echo Use "CREATE EXTENSION pldotnet" to load this file. \quit

-- .NET C# language
CREATE FUNCTION plcsharp_call_handler()
  RETURNS language_handler AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT;

-- comment out if VERSION < 9.0
CREATE FUNCTION plcsharp_inline_handler(internal)
  RETURNS VOID AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION plcsharp_validator(oid)
  RETURNS VOID AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT;

CREATE LANGUAGE plcsharp
  HANDLER plcsharp_call_handler
  INLINE plcsharp_inline_handler -- comment out if VERSION < 9.0
  VALIDATOR plcsharp_validator;

-- .NET F# language
CREATE FUNCTION plfsharp_call_handler()
  RETURNS language_handler AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT;

-- comment out if VERSION < 9.0
CREATE FUNCTION plfsharp_inline_handler(internal)
  RETURNS VOID AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION plfsharp_validator(oid)
  RETURNS VOID AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT;

CREATE LANGUAGE plfsharp
  HANDLER plfsharp_call_handler
  INLINE plfsharp_inline_handler -- comment out if VERSION < 9.0
  VALIDATOR plfsharp_validator;

--within 'pldotnet' schema (do we need this ???)
CREATE TABLE init (module text);

-- PL template installation: (do we need this ???)
INSERT INTO pg_catalog.pg_pltemplate
  SELECT 'plcsharp', false, true, 'plcsharp_call_handler',
    'plcsharp_inline_handler', 'plcsharp_validator', 'MODULE_PATHNAME', NULL
  WHERE 'plcsharp' NOT IN (SELECT tmplname FROM pg_catalog.pg_pltemplate);

INSERT INTO pg_catalog.pg_pltemplate
  SELECT 'plfsharp', false, true, 'plfsharp_call_handler',
    'plfsharp_inline_handler', 'plfsharp_validator', 'MODULE_PATHNAME', NULL
  WHERE 'plfsharp' NOT IN (SELECT tmplname FROM pg_catalog.pg_pltemplate);
