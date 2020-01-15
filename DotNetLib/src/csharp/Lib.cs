/*
 * PL/.NET (pldotnet) - PostgreSQL support for .NET C# and F# as
 *                      procedural languages (PL)
 *
 *
 * Copyright 2019-2020 Brick Abode
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * DotNetLib/src/csharp/Lib.cs - pldotnet assembly compiler and runner
 *
 */
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

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
        static Assembly compiledAssembly;
        static IDictionary<int, (string, MemoryStream)> funcBuiltCodeDict;

        public static int Compile(IntPtr arg, int argLength)
        {

            string spiSrc = @"
                public static class PlDotNet
{
    static List<dynamic> funcExpandDo = new List<dynamic>();

    [DllImport(""@PKG_LIBDIR/pldotnet.so"")]
    public static extern int pl_SPIExecute (string cmd, long limit);

    [StructLayout(LayoutKind.Sequential, Pack=1)]
    public struct PropertyValue
    {
         public IntPtr value;
         public string name;
         public int type;
         public int nrow;
    }

    public static List<dynamic> SPIExecute(string cmd, long limit)
    {
        PlDotNet.pl_SPIExecute(cmd, limit);
        return PlDotNet.funcExpandDo;
    }
    public static T ReadValue<T>(IntPtr handle)
    {
        if (typeof(T) == typeof(string))
        {
            return (T)(object)Marshal.PtrToStringUTF8(handle);
        }
        if (typeof(T) == typeof(decimal))
        {
            return (T)(object)Convert.ToDecimal(Marshal.PtrToStringAnsi(handle));
        }
        return Marshal.PtrToStructure<T>(handle);
    }
    public static void AddProperty(IntPtr arg, int funcoid)
    {
        PropertyValue prop = Marshal.PtrToStructure<PropertyValue>(arg);
        if(PlDotNet.funcExpandDo.Count < prop.nrow + 1)
        {
            PlDotNet.funcExpandDo.Add(new ExpandoObject());
        }
        switch(prop.type)
        {
            case 16: //BOOLOID
                ((IDictionary<String,Object>)PlDotNet.funcExpandDo[prop.nrow])
                    .Add(prop.name, PlDotNet.ReadValue<bool>(prop.value) );
                break;
            case 20: //INT8OID
                ((IDictionary<String,Object>)PlDotNet.funcExpandDo[prop.nrow])
                    .Add(prop.name, PlDotNet.ReadValue<long>(prop.value) );
                break;
            case 21: //INT2OID
                ((IDictionary<String,Object>)PlDotNet.funcExpandDo[prop.nrow])
                    .Add(prop.name, PlDotNet.ReadValue<short>(prop.value) );
                break;
            case 23: //INT4OID
                ((IDictionary<String,Object>)PlDotNet.funcExpandDo[prop.nrow])
                    .Add(prop.name, PlDotNet.ReadValue<int>(prop.value) );
                break;
            case 700: //FLOAT4OID
                ((IDictionary<String,Object>)PlDotNet.funcExpandDo[prop.nrow])
                    .Add(prop.name, PlDotNet.ReadValue<float>(prop.value));
                        break;
            case 701: //FLOAT8OID
                ((IDictionary<String,Object>)PlDotNet.funcExpandDo[prop.nrow])
                    .Add(prop.name, PlDotNet.ReadValue<double>(prop.value));
                break;
            case 1700: //NUMERICOID
                ((IDictionary<String,Object>)PlDotNet.funcExpandDo[prop.nrow])
                    .Add(prop.name, PlDotNet.ReadValue<decimal>(prop.value) );
                break;
            case 1043: //VARCHAROID
                ((IDictionary<String,Object>)PlDotNet.funcExpandDo[prop.nrow])
                    .Add(prop.name, PlDotNet.ReadValue<string>(prop.value) );
                break;
        }
    }
}";
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

            SyntaxTree userTree = SyntaxFactory.ParseSyntaxTree(sourceCode);

            NamespaceDeclarationSyntax parentNamespace =
                userTree.GetRoot().DescendantNodes()
                .OfType<NamespaceDeclarationSyntax>().FirstOrDefault();

            ClassDeclarationSyntax newSPIClassNode =
                SyntaxFactory.ParseSyntaxTree(spiSrc).GetRoot()
                .DescendantNodes().OfType<ClassDeclarationSyntax>()
                .FirstOrDefault();

            SyntaxNode node = userTree.GetRoot().ReplaceNode(parentNamespace,parentNamespace.AddMembers(newSPIClassNode).NormalizeWhitespace());

            userTree = SyntaxFactory.ParseSyntaxTree(node.ToFullString());

            var trustedAssembliesPaths = ((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")).Split(Path.PathSeparator);

            var neededAssemblies = new[]
            {
                "System.Runtime",
                "System.Private.CoreLib",
                "System.Console",
                "System.ObjectModel",
                "netstandard",             /* For Expando/dynamic */
                "System.Linq.Expressions", /* For Expando/dynamic */
                "Microsoft.CSharp",        /* For Expando/dynamic */
            };

            List<PortableExecutableReference> references = trustedAssembliesPaths
                .Where(p => neededAssemblies.Contains(Path.GetFileNameWithoutExtension(p)))
                .Select(p => MetadataReference.CreateFromFile(p))
            .ToList();

            List<String> referencesStr = trustedAssembliesPaths
                .Where(p => neededAssemblies.Contains(Path.GetFileNameWithoutExtension(p)))
            .ToList();

            foreach (var item in referencesStr){
                Console.WriteLine(item);
            }

            CSharpCompilation compilation = CSharpCompilation.Create(
                "plnetproc.dll",
                options: new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary),
                syntaxTrees: new[] { userTree },
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

        public static int AddProperty(IntPtr arg, int argLength)
        {
            if(Lib.compiledAssembly == null)
            {
                Lib.compiledAssembly = Assembly.Load(Lib.memStream.GetBuffer());
            }
            Type procClassType = Lib.compiledAssembly.GetType("DotNetSrc.PlDotNet");
            MethodInfo procMethod = procClassType.GetMethod("AddProperty");
            procMethod.Invoke(null, new object[] {arg, argLength});
            return 0;
        }

        public static int Run(IntPtr arg, int argLength)
        {
            Lib.compiledAssembly = Assembly.Load(Lib.memStream.GetBuffer());
            Type procClassType = Lib.compiledAssembly.GetType("DotNetSrc.ProcedureClass");
            MethodInfo procMethod = procClassType.GetMethod("ProcedureMethod");
            procMethod.Invoke(null, new object[] {arg, argLength});

            return 0;
        }
    }
}
