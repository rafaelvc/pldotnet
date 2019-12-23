CREATE OR REPLACE FUNCTION returnBool() RETURNS boolean AS $$
//PlDotNet.SPIExecute("Select 2", 1);

Assembly compiledAssembly;
compiledAssembly = Assembly.LoadFrom("/var/lib/DotNetLib/src/csharp/Spi.dll");
Type procClassType = compiledAssembly.GetType("DotNetSpi.PlDotNet");
MethodInfo procMethod = procClassType.GetMethod("SPIExecute");
procMethod.Invoke(null, new object[] {"Select 2", 1});

return false;
$$ LANGUAGE plcsharp;
SELECT returnBool();
