using System.Diagnostics;

namespace Tester
{
    internal static class Program
    {
        public static string TestFileName = "test.bat";

        /// <summary>
        ///  The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            // To customize application configuration such as set high DPI settings or default font,
            // see https://aka.ms/applicationconfiguration.

            if (!Configurator.ConfigExists)
            {
                ApplicationConfiguration.Initialize();
                Application.Run(new Tester(ExecuteTestScript));
            }
            else
            {
                ExecuteTestScript(Configurator.Config);
            }
        }

        static void ExecuteTestScript(Config config)
        {
//            string script = $@"
//                make
//OSFMount -a -t file -f ""{config.SdCardImageLocation}"" -o rw -m {config.SdCardMountDriveLetter}:
//copy agi.cx16 * ""{config.SdCardMountDriveLetter}:""
//copy agi.cx16 * ""{config.Cx16EmulatatorFolder}""
//rename agi.cx16* AGI.CX16*
//timeout 3
//OSFMount - D - m ""{config.SdCardMountDriveLetter}""
//            cd ""{config.Cx16EmulatatorFolder}""
//x16emu.exe - sdcard ""{config.SdCardImageLocation}"" - prg ""agi.cx16"" - run - debug d - warp
//cd {Path.GetDirectoryName(Configurator.ConfigFileLocation)}
//            ";

            ProcessStartInfo processStartInfo = GetProcessStartInfo(config);
            processStartInfo.FileName = TestFileName;

            Process.Start(processStartInfo);
        }

        static ProcessStartInfo GetProcessStartInfo(Config config)
        {
            ProcessStartInfo info = new ProcessStartInfo();

            info.UseShellExecute = true;
            info.ArgumentList.Add(config.SdCardImageLocation);
            info.ArgumentList.Add(config.SdCardMountDriveLetter);
            info.ArgumentList.Add(config.Cx16EmulatatorFolder);

            return info; 
        }
    }
}