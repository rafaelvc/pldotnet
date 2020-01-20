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
 * DotNetEngine/src/csharp/Lib.cs - pldotnet assembly compiler and runner
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

namespace PlDotNET
{
    public static class Engine
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
                public static class SPI
{
    static List<dynamic> funcExpandDo = new List<dynamic>();

    [DllImport(""@PKG_LIBDIR/pldotnet.so"")]
    public static extern int pldotnet_SPIExecute (string cmd, long limit);

    [StructLayout(LayoutKind.Sequential, Pack=1)]
    public struct PropertyValue
    {
         public IntPtr value;
         public string name;
         public int type;
         public int nrow;
    }

    public static List<dynamic> Execute(string cmd, long limit)
    {
        SPI.pldotnet_SPIExecute(cmd, limit);
        return SPI.funcExpandDo;
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
        if(SPI.funcExpandDo.Count < prop.nrow + 1)
        {
            SPI.funcExpandDo.Add(new ExpandoObject());
        }
        switch((TypeOid)prop.type)
        {
            case TypeOid.BOOLOID:
                ((IDictionary<String,Object>)SPI.funcExpandDo[prop.nrow])
                    .Add(prop.name, SPI.ReadValue<bool>(prop.value) );
                break;
            case TypeOid.INT2OID:
                ((IDictionary<String,Object>)SPI.funcExpandDo[prop.nrow])
                    .Add(prop.name, SPI.ReadValue<short>(prop.value) );
                break;
            case TypeOid.INT4OID:
                ((IDictionary<String,Object>)SPI.funcExpandDo[prop.nrow])
                    .Add(prop.name, SPI.ReadValue<int>(prop.value) );
                break;
            case TypeOid.INT8OID:
                ((IDictionary<String,Object>)SPI.funcExpandDo[prop.nrow])
                    .Add(prop.name, SPI.ReadValue<long>(prop.value) );
                break;
            case TypeOid.FLOAT4OID:
                ((IDictionary<String,Object>)SPI.funcExpandDo[prop.nrow])
                    .Add(prop.name, SPI.ReadValue<float>(prop.value));
                        break;
            case TypeOid.FLOAT8OID:
                ((IDictionary<String,Object>)SPI.funcExpandDo[prop.nrow])
                    .Add(prop.name, SPI.ReadValue<double>(prop.value));
                break;
            case TypeOid.NUMERICOID:
                ((IDictionary<String,Object>)SPI.funcExpandDo[prop.nrow])
                    .Add(prop.name, SPI.ReadValue<decimal>(prop.value) );
                break;
            case TypeOid.VARCHAROID:
                ((IDictionary<String,Object>)SPI.funcExpandDo[prop.nrow])
                    .Add(prop.name, SPI.ReadValue<string>(prop.value) );
                break;
        }
    }
}";
            LibArgs libArgs = Marshal.PtrToStructure<LibArgs>(arg);
            string sourceCode = Marshal.PtrToStringAuto(libArgs.SourceCode);
            if (Engine.funcBuiltCodeDict == null)
                Engine.funcBuiltCodeDict = new Dictionary<int, (string, MemoryStream)>();
            else {
                // Code has not changed then it is not needed to build it
                try {
                    Engine.funcBuiltCodeDict.TryGetValue(libArgs.FuncOid,
                    out (string src, MemoryStream builtCode) pair);
                    if  (pair.src == sourceCode) {
                        Engine.memStream = pair.builtCode;
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
                "System.ObjectModel",      /* For Expando/dynamic */
                "netstandard",             /* For Expando/dynamic */
                "System.Linq.Expressions", /* For Expando/dynamic */
                "Microsoft.CSharp",        /* For Expando/dynamic */
            };

            List<PortableExecutableReference> references = trustedAssembliesPaths
                .Where(p => neededAssemblies.Contains(Path.GetFileNameWithoutExtension(p)))
                .Select(p => MetadataReference.CreateFromFile(p))
            .ToList();

            CSharpCompilation compilation = CSharpCompilation.Create(
                "plnetproc.dll",
                options: new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary),
                syntaxTrees: new[] { userTree },
                references: references);

            Engine.memStream = new MemoryStream();
            Microsoft.CodeAnalysis.Emit.EmitResult compileResult = compilation.Emit(Engine.memStream);

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

            funcBuiltCodeDict[libArgs.FuncOid] = (sourceCode, Engine.memStream);

            return 0;
        }

        public static int InvokeAddProperty(IntPtr arg, int argLength)
        {
            if(Engine.compiledAssembly == null)
            {
                Engine.compiledAssembly = Assembly.Load(Engine.memStream.GetBuffer());
            }
            Type procClassType = Engine.compiledAssembly.GetType("PlDotNETUserSpace.SPI");
            MethodInfo procMethod = procClassType.GetMethod("AddProperty");
            procMethod.Invoke(null, new object[] {arg, argLength});
            return 0;
        }

        public static int Run(IntPtr arg, int argLength)
        {
            Engine.compiledAssembly = Assembly.Load(Engine.memStream.GetBuffer());
            Type procClassType = Engine.compiledAssembly.GetType("PlDotNETUserSpace.UserClass");
            MethodInfo procMethod = procClassType.GetMethod("CallFunction");
            procMethod.Invoke(null, new object[] {arg, argLength});

            return 0;
        }
    }
}
