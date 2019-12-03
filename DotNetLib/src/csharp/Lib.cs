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
            public int FuncOid;
        }                                  
                                           
        static MemoryStream memStream;
        static IDictionary<int, (string, MemoryStream)> funcBuiltCodeDict;

        public static int Compile(IntPtr arg, int argLength)
        {                                  
            LibArgs libArgs = Marshal.PtrToStructure<LibArgs>(arg);
            string sourceCode = Marshal.PtrToStringAuto(libArgs.SourceCode);

            if (Lib.funcBuiltCodeDict == null)
                Lib.funcBuiltCodeDict = new Dictionary<int, (string, MemoryStream)>();
            else {
                // Code has not changed then it is not needed to build it
                try {
                    Lib.funcBuiltCodeDict.TryGetValue(libArgs.FuncOid,
                    out (string src, MemoryStream builtCode) pair);
                    if  (pair.src == sourceCode) {
                        Lib.memStream = pair.builtCode;
                        return 0;
                    }
                }catch{}
            }

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
                return 0;
            }

            funcBuiltCodeDict[libArgs.FuncOid] = (sourceCode, Lib.memStream);

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
