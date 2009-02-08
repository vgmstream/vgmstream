using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.IO;

namespace Generic_Header_Creator_4
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        // Form Load, we will get every filename in the folder and
        // give it to the ListBox
        private void Form1_Load(object sender, EventArgs e)
        {
            string[] filenames = Directory.GetFiles(Path.GetDirectoryName(Application.ExecutablePath), "*.*");
            foreach (string f in filenames)
                this.listBox1.Items.Add(Path.GetFileName(f));
        }

        
        // Select a file by clicking the ListBox
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {   // Give the selected name to the "Input File" TextBox
            this.txtInputFileCreator.Text = this.listBox1.Text;
            // Set up the Output Name into a TextBox, which can be changed
            this.txtGENHOutputNameCreator.Text = (Path.GetFileNameWithoutExtension(this.txtInputFileCreator.Text) + ".GENH");
            // Open the Input File as "FileStream" from the previous first TextBox
            // FileStream strInputFileCreator = new FileStream(Path.GetFullPath(this.txtInputFileCreator.Text), FileMode.Open, FileAccess.Read);
        }

        private void cmdCreateGENH_Click(object sender, EventArgs e)
        {

            // Call the Export Routine with given Values
            // Name_Of_The_Function(Input_File,Start_Offset,Export_Length,File_To_Write_To)
            FileStream strInputFileCreator = new FileStream(Path.GetFullPath(this.txtInputFileCreator.Text), FileMode.Open, FileAccess.Read);
            FileStream strOutputFileCreator = new FileStream(Path.GetFullPath(this.txtGENHOutputNameCreator.Text), FileMode.Create, FileAccess.Write);

            // Place checks for values and all needed stuff here
            int GENHHeaderSkip = int.Parse(this.txtHeaderSkipCreator.Text);
            int GENHChannels = int.Parse(this.txtChannelsCreator.Text);
            int GENHInterleave = int.Parse(this.txtInterleaveCreator.Text);
            int GENHFrequency = int.Parse(this.txtFrequencyCreator.Text);



            BinaryWriter bw = new BinaryWriter(strOutputFileCreator);
            int strGENH = 0x484E4547; //HNEG (GENH)
            bw.Write(strGENH);
            bw.Write(GENHChannels);
            bw.Write(GENHInterleave);
            // flush and close
            bw.Flush();
            bw.Close();

            // Call the "Export Routine"
            ExtractChunkToFile(strInputFileCreator, 0, (int)strInputFileCreator.Length, this.txtGENHOutputNameCreator.Text, 4096);

            // Close the Input File after processing
            strInputFileCreator.Close();
            strInputFileCreator.Dispose();
        }



        // This is the "Export Routine", all needed values were calculated earlier
        private void ExtractChunkToFile(Stream strInputFileCreator, long pOffset, int pLength, string strOutputFileCreator, int headerSkip)
        {
            BinaryWriter bw = null;

            try
            {   // Open the Output File
                bw = new BinaryWriter(File.Open(strOutputFileCreator, FileMode.Create, FileAccess.Write));

                int read = 0;
                int totalBytes = 0;
                byte[] bytes = new byte[2048];
                strInputFileCreator.Seek((long)pOffset, SeekOrigin.Begin);

                // write empty vals
                bw.Write(new byte[headerSkip], 0, headerSkip);

                int maxread = pLength > bytes.Length ? bytes.Length : pLength;

                while ((read = strInputFileCreator.Read(bytes, 0, maxread)) > 0)
                {
                    bw.Write(bytes, 0, read);
                    totalBytes += read;

                    maxread = (pLength - totalBytes) > bytes.Length ? bytes.Length : (pLength - totalBytes);
                }
            }
            finally
            {
                if (bw != null)
                {
                    bw.Close();
                }
            }
        }
    }
}
