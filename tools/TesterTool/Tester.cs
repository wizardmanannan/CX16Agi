namespace Tester
{
    public partial class Tester : Form
    {
        private Action<Config> RunTest { get; set; }

        public Tester(Action<Config> runTest)
        {
            RunTest = runTest;

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
            };


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
    }
}