\echo Use "CREATE EXTENSION pldotnet" to load this file. \quit

CREATE FUNCTION pldotnet_call_handler()
  RETURNS language_handler AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT;

-- comment out if VERSION < 9.0
CREATE FUNCTION pldotnet_inline_handler(internal)
  RETURNS VOID AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION pldotnet_validator(oid)
  RETURNS VOID AS 'MODULE_PATHNAME'
  LANGUAGE C IMMUTABLE STRICT;

CREATE LANGUAGE pldotnet
  HANDLER pldotnet_call_handler
  INLINE pldotnet_inline_handler -- comment out if VERSION < 9.0
  VALIDATOR pldotnet_validator;

--within 'pldotnet' schema (do we need this ???)
CREATE TABLE init (module text);

-- PL template installation: (do we need this ???)
INSERT INTO pg_catalog.pg_pltemplate
  SELECT 'pldotnet', false, true, 'pldotnet_call_handler',
    'pldotnet_inline_handler', 'pldotnet_validator', 'MODULE_PATHNAME', NULL
  WHERE 'pldotnet' NOT IN (SELECT tmplname FROM pg_catalog.pg_pltemplate);
