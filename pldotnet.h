/*
 * PL/DotNet
 * Author: Brick Abode
 * Version: 0.0.1
 * Please check copyright notice at the bottom of this file
 */

#include "pldotnetcommon.h"

char fs_block_call1[] = "\n\
namespace DotNetLib      \n\
    open System.Runtime.InteropServices\n\
    [<Struct>]           \n\
    [<StructLayout (LayoutKind.Sequential)>]\n\
    type LibArgs =\n";
/****** fs_block_call2 ******
 *      val mutable arg1:int
 *      val mutable arg2:int
 *      ...
 *      val mutable resu:int
 */
char fs_block_call3[] = "\n\
    type Lib =\n";
/********* fs_block_call4 ******
 *         static member <function_name> =
 *             <function_body>
 */
char fs_block_call5[] = "\n\
        static member ProcedureMethod (arg:System.IntPtr) (argLength:int) = \n\
           let mutable libArgs = Marshal.PtrToStructure<LibArgs> arg\n";

char fs_block_call7[] = "\n\
           Marshal.StructureToPtr(libArgs, arg, false)\n\
           0";

//Datum DotNET_callhandler (/*additional args,*/ FunctionCallInfo fcinfo);
//Datum DotNET_validator (/*additional args,*/ Oid oid);
//#if PG_VERSION_NUM >= 90000
//Datum DotNET_inlinehandler (/*additional args,*/ const char *source);
//#endif
