namespace Generic_Header_Creator_4
{
    partial class Form1
    {
        /// <summary>
        /// Erforderliche Designervariable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Verwendete Ressourcen bereinigen.
        /// </summary>
        /// <param name="disposing">True, wenn verwaltete Ressourcen gelöscht werden sollen; andernfalls False.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Vom Windows Form-Designer generierter Code

        /// <summary>
        /// Erforderliche Methode für die Designerunterstützung.
        /// Der Inhalt der Methode darf nicht mit dem Code-Editor geändert werden.
        /// </summary>
        private void InitializeComponent()
        {
            this.listBox1 = new System.Windows.Forms.ListBox();
            this.label1 = new System.Windows.Forms.Label();
            this.txtInputFileCreator = new System.Windows.Forms.TextBox();
            this.label2 = new System.Windows.Forms.Label();
            this.txtGENHOutputNameCreator = new System.Windows.Forms.TextBox();
            this.cmdCreateGENH = new System.Windows.Forms.Button();
            this.label3 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.txtHeaderSkipCreator = new System.Windows.Forms.TextBox();
            this.txtFrequencyCreator = new System.Windows.Forms.TextBox();
            this.txtChannelsCreator = new System.Windows.Forms.TextBox();
            this.txtInterleaveCreator = new System.Windows.Forms.TextBox();
            this.label5 = new System.Windows.Forms.Label();
            this.label6 = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // listBox1
            // 
            this.listBox1.FormattingEnabled = true;
            this.listBox1.Location = new System.Drawing.Point(12, 90);
            this.listBox1.Name = "listBox1";
            this.listBox1.Size = new System.Drawing.Size(320, 212);
            this.listBox1.TabIndex = 0;
            this.listBox1.SelectedIndexChanged += new System.EventHandler(this.listBox1_SelectedIndexChanged);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(9, 9);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(57, 13);
            this.label1.TabIndex = 1;
            this.label1.Text = "File Name:";
            // 
            // txtInputFileCreator
            // 
            this.txtInputFileCreator.Enabled = false;
            this.txtInputFileCreator.Location = new System.Drawing.Point(12, 25);
            this.txtInputFileCreator.Name = "txtInputFileCreator";
            this.txtInputFileCreator.Size = new System.Drawing.Size(320, 20);
            this.txtInputFileCreator.TabIndex = 2;
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(12, 48);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(73, 13);
            this.label2.TabIndex = 3;
            this.label2.Text = "Output Name:";
            // 
            // txtGENHOutputNameCreator
            // 
            this.txtGENHOutputNameCreator.Location = new System.Drawing.Point(12, 64);
            this.txtGENHOutputNameCreator.Name = "txtGENHOutputNameCreator";
            this.txtGENHOutputNameCreator.Size = new System.Drawing.Size(320, 20);
            this.txtGENHOutputNameCreator.TabIndex = 4;
            // 
            // cmdCreateGENH
            // 
            this.cmdCreateGENH.Location = new System.Drawing.Point(12, 308);
            this.cmdCreateGENH.Name = "cmdCreateGENH";
            this.cmdCreateGENH.Size = new System.Drawing.Size(320, 24);
            this.cmdCreateGENH.TabIndex = 5;
            this.cmdCreateGENH.Text = "Create GENeric Header";
            this.cmdCreateGENH.UseVisualStyleBackColor = true;
            this.cmdCreateGENH.Click += new System.EventHandler(this.cmdCreateGENH_Click);
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(366, 74);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(54, 13);
            this.label3.TabIndex = 6;
            this.label3.Text = "Channels:";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(366, 25);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(69, 13);
            this.label4.TabIndex = 7;
            this.label4.Text = "Header Skip:";
            // 
            // txtHeaderSkipCreator
            // 
            this.txtHeaderSkipCreator.Location = new System.Drawing.Point(369, 41);
            this.txtHeaderSkipCreator.Name = "txtHeaderSkipCreator";
            this.txtHeaderSkipCreator.Size = new System.Drawing.Size(100, 20);
            this.txtHeaderSkipCreator.TabIndex = 8;
            // 
            // txtFrequencyCreator
            // 
            this.txtFrequencyCreator.Location = new System.Drawing.Point(480, 90);
            this.txtFrequencyCreator.Name = "txtFrequencyCreator";
            this.txtFrequencyCreator.Size = new System.Drawing.Size(100, 20);
            this.txtFrequencyCreator.TabIndex = 9;
            // 
            // txtChannelsCreator
            // 
            this.txtChannelsCreator.Location = new System.Drawing.Point(369, 90);
            this.txtChannelsCreator.Name = "txtChannelsCreator";
            this.txtChannelsCreator.Size = new System.Drawing.Size(100, 20);
            this.txtChannelsCreator.TabIndex = 10;
            // 
            // txtInterleaveCreator
            // 
            this.txtInterleaveCreator.Location = new System.Drawing.Point(480, 41);
            this.txtInterleaveCreator.Name = "txtInterleaveCreator";
            this.txtInterleaveCreator.Size = new System.Drawing.Size(100, 20);
            this.txtInterleaveCreator.TabIndex = 11;
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(477, 74);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(60, 13);
            this.label5.TabIndex = 12;
            this.label5.Text = "Frequency:";
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(477, 25);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(57, 13);
            this.label6.TabIndex = 13;
            this.label6.Text = "Interleave:";
            // 
            // Form1
            // 
            this.ClientSize = new System.Drawing.Size(652, 355);
            this.Controls.Add(this.label6);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.txtInterleaveCreator);
            this.Controls.Add(this.txtChannelsCreator);
            this.Controls.Add(this.txtFrequencyCreator);
            this.Controls.Add(this.txtHeaderSkipCreator);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.cmdCreateGENH);
            this.Controls.Add(this.txtGENHOutputNameCreator);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.txtInputFileCreator);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.listBox1);
            this.Name = "Form1";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ListBox listBox1;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox txtInputFileCreator;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.TextBox txtGENHOutputNameCreator;
        private System.Windows.Forms.Button cmdCreateGENH;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.TextBox txtHeaderSkipCreator;
        private System.Windows.Forms.TextBox txtFrequencyCreator;
        private System.Windows.Forms.TextBox txtChannelsCreator;
        private System.Windows.Forms.TextBox txtInterleaveCreator;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.Label label6;










    }
}

