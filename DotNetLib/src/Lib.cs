using System;   
using System.Collections.Generic;          
using System.Globalization;                
using System.IO;                           
using System.Linq;                         
using System.Reflection;                   
using System.Runtime.InteropServices;      
using Microsoft.CodeAnalysis;              
using Microsoft.CodeAnalysis.CSharp;       
using Microsoft.CodeAnalysis.Text;         
                                           
namespace DotNetLib                        
{                                          
    public static class Lib                
    {                                      
	[StructLayout(LayoutKind.Sequential)]  
        public struct LibArgs              
        {                                  
            public IntPtr SourceCode;      
            public int Number;             
        }                                  
                                           
	static MemoryStream memStream;         
                                           
        public static int Compile(IntPtr arg, int argLength)
        {                                  
            LibArgs libArgs = Marshal.PtrToStructure<LibArgs>(arg);
            string sourceCode = Marshal.PtrToStringAuto(libArgs.SourceCode);
           
            //Console.WriteLine($"Source Code: {sourceCode}");
	    SyntaxTree tree = SyntaxFactory.ParseSyntaxTree(sourceCode);

	    var trustedAssembliesPaths = ((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")).Split(Path.PathSeparator);

	    var neededAssemblies = new[]
	    {
	        "System.Runtime",
	        "System.Private.CoreLib",
	        "System.Console",
	    };

	    List<PortableExecutableReference> references = trustedAssembliesPaths
                .Where(p => neededAssemblies.Contains(Path.GetFileNameWithoutExtension(p)))
                .Select(p => MetadataReference.CreateFromFile(p))
		.ToList();

            CSharpCompilation compilation = CSharpCompilation.Create(
                "plnetproc.dll",
		options: new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary),
                syntaxTrees: new[] { tree },
                references: references);
                                           
            Lib.memStream = new MemoryStream();
            Microsoft.CodeAnalysis.Emit.EmitResult compileResult = compilation.Emit(Lib.memStream);

	    if(!compileResult.Success)
	    {
	        Console.WriteLine("\n********ERROR************\n");
	        foreach(var diagnostic in compileResult.Diagnostics)
	        {
	            Console.WriteLine(diagnostic.ToString());
	        }
	        Console.WriteLine("\n********ERROR************\n");
	    }

	    string path = "CompiledProcCode.dll";
	    using(FileStream stream = new FileStream(path, FileMode.OpenOrCreate))
	    {                                  
                Microsoft.CodeAnalysis.Emit.EmitResult compileResultFile = compilation.Emit(stream);
	    }                                  
                                           
	    return 0;
	}                                      
                                           
	public static int Run(IntPtr arg, int argLength)
	{                                      
            Assembly compiledAssembly;     
            compiledAssembly = Assembly.Load(Lib.memStream.GetBuffer());
          	                               
	    Type procClassType = compiledAssembly.GetType("DotNetLib.ProcedureClass");
            MethodInfo procMethod = procClassType.GetMethod("ProcedureMethod");
            procMethod.Invoke(null, new object[] {arg, argLength});
                                           
 	    return 0;                          
        }                                  
    }                                      
}

