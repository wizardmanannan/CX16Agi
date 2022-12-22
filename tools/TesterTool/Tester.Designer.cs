namespace Tester
{
    partial class Tester
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.label1 = new System.Windows.Forms.Label();
            this.txtSdImageLocation = new System.Windows.Forms.TextBox();
            this.txtSdCardMountDriveLetter = new System.Windows.Forms.TextBox();
            this.label2 = new System.Windows.Forms.Label();
            this.txtCx16EmulatorFolder = new System.Windows.Forms.TextBox();
            this.label3 = new System.Windows.Forms.Label();
            this.Test = new System.Windows.Forms.Button();
            this.txtMakeFileFolder = new System.Windows.Forms.TextBox();
            this.label4 = new System.Windows.Forms.Label();
            this.dlgFile = new System.Windows.Forms.OpenFileDialog();
            this.btnImageLocationFolderBrowse = new System.Windows.Forms.Button();
            this.btnEmulatorFolderBrowse = new System.Windows.Forms.Button();
            this.btnMakeFolderBrowse = new System.Windows.Forms.Button();
            this.dlgFolder = new System.Windows.Forms.FolderBrowserDialog();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(12, 20);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(429, 25);
            this.label1.TabIndex = 0;
            this.label1.Text = "SDCard Image Location (eg. c:\\myfolder\\sdcard.img)";
            // 
            // txtSdImageLocation
            // 
            this.txtSdImageLocation.Location = new System.Drawing.Point(12, 57);
            this.txtSdImageLocation.Name = "txtSdImageLocation";
            this.txtSdImageLocation.Size = new System.Drawing.Size(429, 31);
            this.txtSdImageLocation.TabIndex = 1;
            this.txtSdImageLocation.Leave += new System.EventHandler(this.txtImageLocation_Leave);
            // 
            // txtSdCardMountDriveLetter
            // 
            this.txtSdCardMountDriveLetter.Location = new System.Drawing.Point(12, 134);
            this.txtSdCardMountDriveLetter.Name = "txtSdCardMountDriveLetter";
            this.txtSdCardMountDriveLetter.Size = new System.Drawing.Size(429, 31);
            this.txtSdCardMountDriveLetter.TabIndex = 3;
            this.txtSdCardMountDriveLetter.Text = "E";
            this.txtSdCardMountDriveLetter.Leave += new System.EventHandler(this.txtDriveLetter_Leave);
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(12, 97);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(232, 25);
            this.label2.TabIndex = 2;
            this.label2.Text = "SDCard Mount Letter (eg. E)";
            // 
            // txtCx16EmulatorFolder
            // 
            this.txtCx16EmulatorFolder.Location = new System.Drawing.Point(12, 211);
            this.txtCx16EmulatorFolder.Name = "txtCx16EmulatorFolder";
            this.txtCx16EmulatorFolder.Size = new System.Drawing.Size(429, 31);
            this.txtCx16EmulatorFolder.TabIndex = 5;
            this.txtCx16EmulatorFolder.Leave += new System.EventHandler(this.txtEmulator_Leave);
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(12, 174);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(432, 25);
            this.label3.TabIndex = 4;
            this.label3.Text = "CommanderX16 Emulator Folder (eg. c:\\commander)";
            // 
            // Test
            // 
            this.Test.Location = new System.Drawing.Point(12, 325);
            this.Test.Name = "Test";
            this.Test.Size = new System.Drawing.Size(112, 34);
            this.Test.TabIndex = 6;
            this.Test.Text = "Test";
            this.Test.UseVisualStyleBackColor = true;
            this.Test.Click += new System.EventHandler(this.Test_Click);
            // 
            // txtMakeFileFolder
            // 
            this.txtMakeFileFolder.Location = new System.Drawing.Point(12, 288);
            this.txtMakeFileFolder.Name = "txtMakeFileFolder";
            this.txtMakeFileFolder.Size = new System.Drawing.Size(429, 31);
            this.txtMakeFileFolder.TabIndex = 8;
            this.txtMakeFileFolder.TextChanged += new System.EventHandler(this.txtMakeFileFolder_TextChanged);
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(12, 251);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(316, 25);
            this.label4.TabIndex = 7;
            this.label4.Text = "MakeFile Folder (eg. c:\\myCodeFolder)";
            // 
            // dlgFile
            // 
            this.dlgFile.FileName = "openFileDialog1";
            this.dlgFile.FileOk += new System.ComponentModel.CancelEventHandler(this.dlgFile_FileOk);
            // 
            // btnImageLocationFolderBrowse
            // 
            this.btnImageLocationFolderBrowse.Location = new System.Drawing.Point(462, 57);
            this.btnImageLocationFolderBrowse.Name = "btnImageLocationFolderBrowse";
            this.btnImageLocationFolderBrowse.Size = new System.Drawing.Size(112, 31);
            this.btnImageLocationFolderBrowse.TabIndex = 9;
            this.btnImageLocationFolderBrowse.Text = "Browse";
            this.btnImageLocationFolderBrowse.UseVisualStyleBackColor = true;
            this.btnImageLocationFolderBrowse.Click += new System.EventHandler(this.btnImageLocationFolderBrowse_Click);
            // 
            // btnEmulatorFolderBrowse
            // 
            this.btnEmulatorFolderBrowse.Location = new System.Drawing.Point(462, 211);
            this.btnEmulatorFolderBrowse.Name = "btnEmulatorFolderBrowse";
            this.btnEmulatorFolderBrowse.Size = new System.Drawing.Size(112, 31);
            this.btnEmulatorFolderBrowse.TabIndex = 10;
            this.btnEmulatorFolderBrowse.Text = "Browse";
            this.btnEmulatorFolderBrowse.UseVisualStyleBackColor = true;
            this.btnEmulatorFolderBrowse.Click += new System.EventHandler(this.btnEmulatorFolderBrowse_Click);
            // 
            // btnMakeFolderBrowse
            // 
            this.btnMakeFolderBrowse.Location = new System.Drawing.Point(462, 288);
            this.btnMakeFolderBrowse.Name = "btnMakeFolderBrowse";
            this.btnMakeFolderBrowse.Size = new System.Drawing.Size(112, 31);
            this.btnMakeFolderBrowse.TabIndex = 11;
            this.btnMakeFolderBrowse.Text = "Browse";
            this.btnMakeFolderBrowse.UseVisualStyleBackColor = true;
            this.btnMakeFolderBrowse.Click += new System.EventHandler(this.btnMakeFolderBrowse_Click);
            // 
            // Tester
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(10F, 25F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(637, 374);
            this.Controls.Add(this.btnMakeFolderBrowse);
            this.Controls.Add(this.btnEmulatorFolderBrowse);
            this.Controls.Add(this.btnImageLocationFolderBrowse);
            this.Controls.Add(this.txtMakeFileFolder);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.Test);
            this.Controls.Add(this.txtCx16EmulatorFolder);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.txtSdCardMountDriveLetter);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.txtSdImageLocation);
            this.Controls.Add(this.label1);
            this.Name = "Tester";
            this.Text = "Meka Agi Tester";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private Label label1;
        private TextBox txtSdImageLocation;
        private TextBox txtSdCardMountDriveLetter;
        private Label label2;
        private TextBox txtCx16EmulatorFolder;
        private Label label3;
        private Button Test;
        private TextBox txtMakeFileFolder;
        private Label label4;
        private OpenFileDialog dlgFile;
        private Button btnImageLocationFolderBrowse;
        private Button btnEmulatorFolderBrowse;
        private Button btnMakeFolderBrowse;
        private FolderBrowserDialog dlgFolder;
    }
}