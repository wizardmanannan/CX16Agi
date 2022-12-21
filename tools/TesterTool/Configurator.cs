using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace Tester
{
    internal static class Configurator
    {
        const string SdCardImageLocationKey = "SdCardImageLocationKey";
        const string SdCardMountDriveLetterKey = "SdCardMountDriveLetterKey";
        const string Cx16EmulatatorFolderKey = "Cx16EmulatatorFolderKey";

        const string ConfigFileName = "cx16TesterConfig.json";

        static string _failToWriteError;

        static Configurator()
        {
            _failToWriteError = $"Failed To Write To {ConfigFileLocation}. Ensure your write permissions.";
        }

        public static string ConfigFileLocation {
                get {
                return Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), ConfigFileName);
                }
            }

        public static bool ConfigExists
        {
            get
            {
                return File.Exists(ConfigFileLocation); 
            }
        }

        public static bool CreateNewConfiguration(string sdCardImageLocation, string sdCardMountDriveLetter, string cx16EmulatatorFolder, out string error)
        {
            Dictionary<string, string> config = new Dictionary<string, string>();

            config[SdCardImageLocationKey] = sdCardImageLocation;
            config[SdCardMountDriveLetterKey] = sdCardMountDriveLetter;
            config[Cx16EmulatatorFolderKey] = cx16EmulatatorFolder;

            try
            {
                File.WriteAllText(ConfigFileLocation, JsonConvert.SerializeObject(config));
            }
            catch (Exception ex)
            {
                error = $"{_failToWriteError} \n {ex.Message}";
                return false;
            }
              
            error = string.Empty;
            return true; 
        }
    }
}
