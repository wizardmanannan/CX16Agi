using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Tester
{
    internal static class Program
    {
        public static string TestFileName = "test.bat";

        private static string _testFilePath = "";


    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool AllocConsole();

    /// <summary>
    ///  The main entry point for the application.
    /// </summary>
    [STAThread]
        async static Task Main()
        {
            try
            {
                // To customize application configuration such as set high DPI settings or default font,
                // see https://aka.ms/applicationconfiguration.
                AllocConsole();
               _testFilePath = SetUpBatchFile(TestFileName);
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
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
                await Task.Delay(-1); 
            }
        }

        private static string SetUpBatchFile(string resourceName)
        {
            string result = $"{Path.GetTempFileName()}.bat";
            try
            {
                File.WriteAllText(result, Resources.test_bat);
            }
            catch (Exception ex)
            {
                throw new Exception($"Unable to write to temp directory due to {ex.Message}");
            }

            return result;
        }

        static void ExecuteTestScript(Config config)
        {
            ProcessStartInfo processStartInfo = GetProcessStartInfo(config);
            processStartInfo.FileName = _testFilePath;

            Process.Start(processStartInfo);
        }

        static ProcessStartInfo GetProcessStartInfo(Config config)
        {
            ProcessStartInfo info = new ProcessStartInfo();

            info.UseShellExecute = true;
            info.ArgumentList.Add(config.SdCardImageLocation);
            info.ArgumentList.Add(config.SdCardMountDriveLetter);
            info.ArgumentList.Add(config.Cx16EmulatatorFolder);
            info.ArgumentList.Add(config.MakeFileFolder);

            return info; 
        }
    }
}