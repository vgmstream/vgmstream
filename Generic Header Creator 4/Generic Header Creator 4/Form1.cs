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

            FileInfo fi = new FileInfo(Path.GetFullPath(this.txtInputFileCreator.Text));
            string InputFileLen = fi.Length.ToString();
            this.txtInputFileLen.Text = InputFileLen;
        }



        private void cmdCreateGENH_Click(object sender, EventArgs e)
        {
            FileStream strInputFileCreator = new FileStream(Path.GetFullPath(this.txtInputFileCreator.Text), FileMode.Open, FileAccess.Read);



            // Place checks for values and all needed stuff here
            int GENHToken = 0x484E4547; //HNEG (GENH)
            int GENHChannels = int.Parse(this.txtChannelsCreator.Text);
            int GENHInterleave = int.Parse(this.txtInterleaveCreator.Text);
            int GENHFrequency = int.Parse(this.txtFrequencyCreator.Text);

            int GENHLoopStart = int.Parse(this.txtLoopStartCreator.Text);
            int GENHLoopEnd = int.Parse(this.txtLoopEndCreator.Text);
            int GENHIdentiferByte = (this.comboFileFomat.SelectedIndex);
            int GENHHeaderSkip = int.Parse(this.txtHeaderSkipCreator.Text);
            //int GENHFileStartOffset;
            
                
            // Call the Export Routine with given Values
            // Name_Of_The_Function(Input_File,Start_Offset,Export_Length,File_To_Write_To)
            ExtractChunkToFile(strInputFileCreator, 0, (int)strInputFileCreator.Length, this.txtGENHOutputNameCreator.Text);

            FileStream strOutputFileCreator = new FileStream(Path.GetFullPath(this.txtGENHOutputNameCreator.Text), FileMode.Open, FileAccess.Write);
            BinaryWriter bw = new BinaryWriter(strOutputFileCreator);
            
            bw.Write(GENHToken);            // 0x00
            bw.Write(GENHChannels);         // 0x04
            bw.Write(GENHInterleave);       // 0x08
            bw.Write(GENHFrequency);        // 0x0C
            bw.Write(GENHLoopStart);        // 0x10
            bw.Write(GENHLoopEnd);          // 0x14
            bw.Write(GENHIdentiferByte);    // 0x18

            // flush and close
            bw.Flush();
            bw.Close();


            // Close the Input File after processing
            strInputFileCreator.Close();
            strInputFileCreator.Dispose();

            strOutputFileCreator.Close();
            strOutputFileCreator.Dispose();
        }


        // This is the "Export Routine", all needed values were calculated earlier
        private void ExtractChunkToFile(Stream strInputFileCreator, long pOffset, int pLength, string strOutputFileCreator)
        {
            BinaryWriter bw = null;

            try
            {   // Open the Output File
                bw = new BinaryWriter(File.Open(strOutputFileCreator, FileMode.Create, FileAccess.Write));
                bw.BaseStream.Position = 0x1000;
                int read = 0;
                int totalBytes = 0;
                byte[] bytes = new byte[2048];
                strInputFileCreator.Seek((long)pOffset, SeekOrigin.Begin);

                // write empty vals
                //bw.Write(new byte[headerSkip], 0, headerSkip);

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

        private void cmdUseFileEnd_Click(object sender, EventArgs e)
        {
            int GENHIdentiferByte = (this.comboFileFomat.SelectedIndex);
            int GENHChannels = int.Parse(this.txtChannelsCreator.Text);
            
          
            FileStream strInputFileCreator = new FileStream(Path.GetFullPath(this.txtInputFileCreator.Text), FileMode.Open, FileAccess.Read);



            switch (GENHIdentiferByte) {
                case 0:

                    break;
                case 1:

                    break;
                default:
    // alles andere interessiert uns nicht
    break;
}


               
        }
    }

        }
    

    