namespace Tester
{
    public partial class Tester : Form
    {
        public const string ErrorIsRequired = "'{0}' is required";
        public const string ErrorFileMustExist = "The file in '{0}' must exist";
        public const string ErrorDirectoryMustExist = "The directory in '{0}' must exist";
        public const string ErrorMountDriveMustNotExist = "The mount drive '{0}' already exists";

        private Action<Config> RunTest { get; set; }
        private string _optionalArgments;

        public Tester(Action<Config> runTest, string optionalArgments)
        {
            RunTest = runTest;
            _optionalArgments = optionalArgments;
            InitializeComponent();
        }

        private void txtImageLocation_Leave(object sender, EventArgs e)
        {
            txtSdImageLocation.Text = txtSdImageLocation.Text.Trim();
            if (string.IsNullOrEmpty(txtCx16EmulatorFolder.Text))
            {
                txtCx16EmulatorFolder.Text = Path.GetDirectoryName(txtSdImageLocation.Text);
            }
        }

        private void txtDriveLetter_Leave(object sender, EventArgs e)
        {
            txtSdCardMountDriveLetter.Text = txtSdCardMountDriveLetter.Text.Trim();
        }

        private void txtEmulator_Leave(object sender, EventArgs e)
        {
            txtCx16EmulatorFolder.Text = txtCx16EmulatorFolder.Text.Trim();
        }

        private void Test_Click(object sender, EventArgs e)
        {
            string error;

            Config config = new Config()
            {
                SdCardImageLocation = txtSdImageLocation.Text,
                SdCardMountDriveLetter = txtSdCardMountDriveLetter.Text,
                Cx16EmulatatorFolder = txtCx16EmulatorFolder.Text,
                MakeFileFolder = txtMakeFileFolder.Text,
                OptionalArguments = _optionalArgments
            };

            if (ValidateChildren())
            {
                if (!Configurator.CreateNewConfiguration(config, out error))
                {
                    MessageBox.Show(error);
                    throw new Exception(error);
                }
                else
                {
                    RunTest(Configurator.Config);
                    Application.Exit();
                }
            }
        }

        private void txtMakeFileFolder_TextChanged(object sender, EventArgs e)
        {
            txtMakeFileFolder.Text = txtMakeFileFolder.Text.Trim();
        }

        private void btnImageLocationFolderBrowse_Click(object sender, EventArgs e)
        {
            BrowseFile(txtSdImageLocation);
        }

        private void BrowseFile(TextBox textBox)
        {
          if(dlgFile.ShowDialog() == DialogResult.OK)
            {
                textBox.Text = dlgFile.FileName;
            }
        }

        private void BrowseFolder(TextBox textBox)
        {
            if (dlgFolder.ShowDialog() == DialogResult.OK)
            {
                textBox.Text = dlgFolder.SelectedPath;
            }
        }



        private void button1_Click(object sender, EventArgs e)
        {

        }

        private void btnEmulatorFolderBrowse_Click(object sender, EventArgs e)
        {
            BrowseFolder(txtCx16EmulatorFolder);
        }

        private void btnMakeFolderBrowse_Click(object sender, EventArgs e)
        {
            BrowseFolder(txtMakeFileFolder);
        }

        private void dlgFile_FileOk(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (string.IsNullOrEmpty(txtCx16EmulatorFolder.Text))
            {
                txtCx16EmulatorFolder.Text = Path.GetDirectoryName(((OpenFileDialog) sender).FileName);
            }
        }

        private void txtSdImageLocation_Validating(object sender, System.ComponentModel.CancelEventArgs e)
        {
            ValidateFunc(txtSdImageLocation, lblSdCardImageLocation.Text, e, File.Exists, ErrorFileMustExist);
        }

        private void ValidateFunc(TextBoxBase textBox, string label, System.ComponentModel.CancelEventArgs e, Func<string, bool> testFunc, string error)
        {
            if (string.IsNullOrEmpty(textBox.Text))
            {
                DisplayErrorForTextBox(ErrorIsRequired, label);
                e.Cancel = true;
            }
            else
            {
                if (!testFunc(textBox.Text))
                {
                    DisplayErrorForTextBox(error, label);
                    e.Cancel = true;
                }
            }
        }

        private void DisplayErrorForTextBox(string error, string label)
        {
            MessageBox.Show(string.Format(error, label), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }

        private void txtCx16EmulatorFolder_Validating(object sender, System.ComponentModel.CancelEventArgs e)
        {
            ValidateFunc(txtCx16EmulatorFolder, lblCx16EmulatorFolder.Text, e, Directory.Exists, ErrorDirectoryMustExist);
        }

        private void txtMakeFileFolder_Validating(object sender, System.ComponentModel.CancelEventArgs e)
        {
            ValidateFunc(txtMakeFileFolder, lblMakeFileFolder.Text, e, Directory.Exists, ErrorDirectoryMustExist);
        }

        private void txtSdCardMountDriveLetter_Validating(object sender, System.ComponentModel.CancelEventArgs e)
        {
            ValidateFunc(txtSdCardMountDriveLetter, lblSdCardMountDriveLetter.Text, e, s=> !Directory.Exists(s + ":\\"), ErrorMountDriveMustNotExist);
        }
    }
}