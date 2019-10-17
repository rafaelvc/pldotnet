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

            string sourceCode = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
                ? Marshal.PtrToStringUni(libArgs.SourceCode)
                : Marshal.PtrToStringUTF8(libArgs.SourceCode); 
           
            //Console.WriteLine($"Source Code: {sourceCode}");
	    SyntaxTree tree = SyntaxFactory.ParseSyntaxTree(sourceCode);
            
	    var sysruntime = MetadataReference.CreateFromFile(@"/usr/share/dotnet/shared/Microsoft.NETCore.App/3.0.0/System.Runtime.dll");
            var sysconsole = MetadataReference.CreateFromFile(@"/usr/share/dotnet/shared/Microsoft.NETCore.App/3.0.0/System.Console.dll");
            var mscorlib = MetadataReference.CreateFromFile(typeof(object).Assembly.Location);

            CSharpCompilation compilation = CSharpCompilation.Create(
                "calc.dll",                
                options: new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary),
                syntaxTrees: new[] { tree },
                references: new[] { mscorlib, sysruntime, sysconsole });
                                           
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
          	                               
	    Type calculator = compiledAssembly.GetType("ProcedureCode.ProcedureClass");
            MethodInfo evaluate = calculator.GetMethod("ProcedureMethod");
            evaluate.Invoke(null, null);
                                           
 	    return 0;                          
        }                                  
    }                                      
}

