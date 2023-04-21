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

        private static Config _config;

        public static Config Config
        {
            get
            {
                if (_config == null)
                {
                    _config = new Config();
                    
                    if (ConfigExists)
                    {
                        _config = JsonConvert.DeserializeObject<Config>(File.ReadAllText(ConfigFileLocation));
                    }
                }
                
                return _config;
            }
        }

        public static bool CreateNewConfiguration(Config config, out string error)
        {
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
