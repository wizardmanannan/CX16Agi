namespace Tester
{
    internal static class Program
    {
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
                Application.Run(new Tester());
            }
        }

        static void ExecuteScript(string sdCardImageLocation, string sdCardMountDriveLetter, string cx16EmulatatorFolder)
        {

        }
    }
}