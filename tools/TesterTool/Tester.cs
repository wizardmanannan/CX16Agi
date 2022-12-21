namespace Tester
{
    public partial class Tester : Form
    {
        public Tester()
        {
            InitializeComponent();
        }

        private void txtImageLocation_Leave(object sender, EventArgs e)
        {
            txtSdImageLocation.Text = txtSdImageLocation.Text.Trim();
            if (string.IsNullOrEmpty(txtCx16EmulatorFolder.Text))
            {
                txtCx16EmulatorFolder.Text = txtSdImageLocation.Text;
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
            if(!Configurator.CreateNewConfiguration(txtSdImageLocation.Text, txtSdCardMountDriveLetter.Text, txtCx16EmulatorFolder.Text, out error))
            {
                MessageBox.Show(error);
            }
            else
            {

            }
        }
    }
}