do $$
using System;
using System.Runtime.InteropServices;
namespace DotNetLib
{
    public static class Lib
    {
        private static int s_CallCount = 1;

        [StructLayout(LayoutKind.Sequential)]
        public struct LibArgs
        {
            public int Number1;
            public int Number2;
            public int Result;
        }
        
        public static int Main(IntPtr arg, int argLength)
        {
            if (argLength < System.Runtime.InteropServices.Marshal.SizeOf(typeof(LibArgs)))
            {
                return 1;
            }
            LibArgs libArgs = Marshal.PtrToStructure<LibArgs>(arg);
            libArgs.Result = libArgs.Number1 + libArgs.Number2;
            Marshal.StructureToPtr<LibArgs>(libArgs, arg, false);
            return 0;
        }
    }
}
$$ language pldotnet;

SELECT 1;
