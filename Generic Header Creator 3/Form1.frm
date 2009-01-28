VERSION 5.00
Object = "{49CBE90A-5F9F-4127-919C-0B74E18CB87E}#1.0#0"; "CK-Tools.ocx"
Begin VB.Form Form1 
   BackColor       =   &H00E0E0E0&
   BorderStyle     =   0  'Kein
   Caption         =   "Generic Header Creator 3.00 for vgmstream"
   ClientHeight    =   8910
   ClientLeft      =   0
   ClientTop       =   0
   ClientWidth     =   13110
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MinButton       =   0   'False
   ScaleHeight     =   594
   ScaleMode       =   3  'Pixel
   ScaleWidth      =   874
   StartUpPosition =   1  'Fenstermitte
   Begin VB.PictureBox Picture5 
      BorderStyle     =   0  'Kein
      Height          =   495
      Left            =   2520
      ScaleHeight     =   495
      ScaleWidth      =   495
      TabIndex        =   127
      Top             =   420
      Width           =   495
   End
   Begin VB.PictureBox Picture4 
      BorderStyle     =   0  'Kein
      Height          =   495
      Left            =   1920
      ScaleHeight     =   495
      ScaleWidth      =   495
      TabIndex        =   126
      Top             =   420
      Width           =   495
   End
   Begin VB.PictureBox Picture3 
      BorderStyle     =   0  'Kein
      Height          =   495
      Left            =   1320
      ScaleHeight     =   495
      ScaleWidth      =   495
      TabIndex        =   125
      Top             =   420
      Width           =   495
   End
   Begin VB.PictureBox Picture2 
      BorderStyle     =   0  'Kein
      Height          =   495
      Left            =   720
      ScaleHeight     =   495
      ScaleWidth      =   495
      TabIndex        =   124
      Top             =   420
      Width           =   495
   End
   Begin VB.PictureBox Picture1 
      BorderStyle     =   0  'Kein
      Height          =   495
      Left            =   120
      ScaleHeight     =   495
      ScaleWidth      =   495
      TabIndex        =   123
      Top             =   420
      Width           =   495
   End
   Begin VB.TextBox txtInputFile 
      Appearance      =   0  '2D
      BackColor       =   &H00C0C0C0&
      Height          =   285
      Left            =   120
      Locked          =   -1  'True
      TabIndex        =   95
      Top             =   10320
      Width           =   4095
   End
   Begin VB.TextBox txtLoopEndCut 
      Appearance      =   0  '2D
      Height          =   285
      Left            =   6000
      TabIndex        =   93
      Top             =   11040
      Width           =   2055
   End
   Begin VB.TextBox txtGetFileName 
      Appearance      =   0  '2D
      Height          =   285
      Left            =   3960
      TabIndex        =   92
      Top             =   10680
      Width           =   4095
   End
   Begin VB.TextBox txtLoopStartCut 
      Appearance      =   0  '2D
      Height          =   285
      Left            =   3960
      TabIndex        =   91
      Top             =   11040
      Width           =   2055
   End
   Begin VB.Frame frmEDITOR 
      BackColor       =   &H00B8B8B8&
      Caption         =   "Editor:"
      BeginProperty Font 
         Name            =   "MS Sans Serif"
         Size            =   8.25
         Charset         =   0
         Weight          =   700
         Underline       =   0   'False
         Italic          =   0   'False
         Strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H00800000&
      Height          =   255
      Left            =   2640
      TabIndex        =   61
      Top             =   10680
      Width           =   1215
      Begin VB.Frame Frame4 
         BackColor       =   &H00B8B8B8&
         Caption         =   "File:"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   6135
         Left            =   360
         TabIndex        =   83
         Top             =   240
         Width           =   4335
         Begin VB.TextBox txtInputFileLengthEditor 
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Enabled         =   0   'False
            Height          =   285
            Left            =   3000
            TabIndex        =   87
            Top             =   840
            Width           =   1215
         End
         Begin VB.TextBox txtInputFileEditor 
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Enabled         =   0   'False
            Height          =   285
            Left            =   120
            TabIndex        =   86
            Top             =   840
            Width           =   2775
         End
         Begin VB.FileListBox File2 
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            ForeColor       =   &H00800000&
            Height          =   4320
            Left            =   120
            Pattern         =   "*.GENH"
            TabIndex        =   85
            Top             =   1200
            Width           =   4095
         End
         Begin VB.CommandButton cmdSaveEditor 
            Caption         =   "Save Changes"
            Height          =   375
            Left            =   120
            TabIndex        =   84
            Top             =   5640
            Width           =   4095
         End
         Begin CK_Tools.FolderBrowser FolderBrowser1Editor 
            Height          =   285
            Left            =   120
            TabIndex        =   88
            Top             =   240
            Width           =   4080
            _ExtentX        =   7197
            _ExtentY        =   503
            BackColor       =   14737632
            BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
               Name            =   "MS Sans Serif"
               Size            =   8.25
               Charset         =   0
               Weight          =   400
               Underline       =   0   'False
               Italic          =   0   'False
               Strikethrough   =   0   'False
            EndProperty
         End
         Begin VB.Label Label7 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "File Length:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   3000
            TabIndex        =   90
            Top             =   600
            Width           =   825
         End
         Begin VB.Label Label8 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Input File Name:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   120
            TabIndex        =   89
            Top             =   600
            Width           =   1155
         End
      End
      Begin VB.Frame Frame6 
         BackColor       =   &H00B8B8B8&
         Caption         =   "Format:"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   1575
         Left            =   4560
         TabIndex        =   79
         Top             =   240
         Width           =   4695
         Begin VB.ComboBox comboFileFormatEditor 
            BackColor       =   &H00E0E0E0&
            Height          =   315
            ItemData        =   "Form1.frx":0000
            Left            =   120
            List            =   "Form1.frx":0028
            TabIndex        =   80
            Top             =   480
            Width           =   4455
         End
         Begin VB.Label Label9 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "File Format:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   120
            TabIndex        =   82
            Top             =   240
            Width           =   810
         End
         Begin VB.Label lblERROREditor 
            Alignment       =   2  'Zentriert
            BackStyle       =   0  'Transparent
            BeginProperty Font 
               Name            =   "MS Sans Serif"
               Size            =   8.25
               Charset         =   0
               Weight          =   700
               Underline       =   0   'False
               Italic          =   0   'False
               Strikethrough   =   0   'False
            EndProperty
            ForeColor       =   &H000000FF&
            Height          =   495
            Left            =   120
            TabIndex        =   81
            Top             =   960
            Width           =   4485
         End
      End
      Begin VB.Frame Frame7 
         BackColor       =   &H00B8B8B8&
         Caption         =   "Options:"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   4455
         Left            =   4560
         TabIndex        =   62
         Top             =   1920
         Width           =   4695
         Begin VB.ComboBox txtGENHFrequencyEditor 
            BackColor       =   &H00E0E0E0&
            Height          =   315
            ItemData        =   "Form1.frx":01A0
            Left            =   2520
            List            =   "Form1.frx":01C5
            TabIndex        =   70
            Top             =   2520
            Width           =   1935
         End
         Begin VB.ComboBox txtGENHInterleaveEditor 
            BackColor       =   &H00E0E0E0&
            Height          =   315
            ItemData        =   "Form1.frx":0215
            Left            =   2520
            List            =   "Form1.frx":0246
            TabIndex        =   69
            Top             =   1920
            Width           =   1935
         End
         Begin VB.ComboBox txtGENHHeaderSkipEditor 
            BackColor       =   &H00E0E0E0&
            Height          =   315
            ItemData        =   "Form1.frx":029D
            Left            =   360
            List            =   "Form1.frx":02C8
            TabIndex        =   68
            Top             =   1920
            Width           =   1935
         End
         Begin VB.TextBox txtGENHLoopStartSamplesEditor 
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Height          =   315
            Left            =   360
            TabIndex        =   67
            Top             =   3360
            Width           =   1935
         End
         Begin VB.TextBox txtGENHLoopEndSamplesEditor 
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Height          =   315
            Left            =   2520
            TabIndex        =   66
            Top             =   3360
            Width           =   1935
         End
         Begin VB.ComboBox txtGENHChannelsEditor 
            BackColor       =   &H00E0E0E0&
            Height          =   315
            ItemData        =   "Form1.frx":030D
            Left            =   360
            List            =   "Form1.frx":0329
            TabIndex        =   65
            Top             =   2520
            Width           =   1935
         End
         Begin VB.TextBox txtEmbeddedFileEditor 
            Alignment       =   2  'Zentriert
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Height          =   285
            Left            =   360
            Locked          =   -1  'True
            TabIndex        =   64
            Top             =   1200
            Width           =   4095
         End
         Begin VB.TextBox txtGENHVersionEditor 
            Alignment       =   2  'Zentriert
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Height          =   285
            Left            =   360
            Locked          =   -1  'True
            TabIndex        =   63
            Top             =   600
            Width           =   4095
         End
         Begin VB.Label Label12 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Header Skip:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   78
            Top             =   1680
            Width           =   930
         End
         Begin VB.Label Label13 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Loop End (Samples):"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   2520
            TabIndex        =   77
            Top             =   3120
            Width           =   1470
         End
         Begin VB.Label Label14 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Loop Start (Samples):"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   76
            Top             =   3120
            Width           =   1515
         End
         Begin VB.Label Label15 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Frequency:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   2520
            TabIndex        =   75
            Top             =   2280
            Width           =   795
         End
         Begin VB.Label Label16 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Interleave:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   2520
            TabIndex        =   74
            Top             =   1680
            Width           =   750
         End
         Begin VB.Label Label17 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Channels:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   73
            Top             =   2280
            Width           =   705
         End
         Begin VB.Label Label18 
            AutoSize        =   -1  'True
            BackStyle       =   0  'Transparent
            Caption         =   "Embedded File:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   72
            Top             =   960
            Width           =   1095
         End
         Begin VB.Label Label6 
            AutoSize        =   -1  'True
            BackStyle       =   0  'Transparent
            Caption         =   "GENH Version:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   71
            Top             =   360
            Width           =   1080
         End
      End
   End
   Begin VB.Frame frmExtractor 
      BackColor       =   &H00B8B8B8&
      Caption         =   "Extractor:"
      BeginProperty Font 
         Name            =   "MS Sans Serif"
         Size            =   8.25
         Charset         =   0
         Weight          =   700
         Underline       =   0   'False
         Italic          =   0   'False
         Strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H00800000&
      Height          =   255
      Left            =   2640
      TabIndex        =   29
      Top             =   11040
      Width           =   1215
      Begin VB.Frame Frame8 
         BackColor       =   &H00B8B8B8&
         Caption         =   "File:"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   6135
         Left            =   120
         TabIndex        =   53
         Top             =   240
         Width           =   4335
         Begin VB.CommandButton cmdExtractEmbeddedFile 
            Caption         =   "Extract Embedded File"
            Height          =   375
            Left            =   120
            TabIndex        =   57
            Top             =   5640
            Width           =   4095
         End
         Begin VB.FileListBox File3 
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            ForeColor       =   &H00800000&
            Height          =   4320
            Left            =   120
            Pattern         =   "*.GENH"
            TabIndex        =   56
            Top             =   1200
            Width           =   4095
         End
         Begin VB.TextBox txtInputFileExtractor 
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Enabled         =   0   'False
            Height          =   285
            Left            =   120
            TabIndex        =   55
            Top             =   840
            Width           =   2775
         End
         Begin VB.TextBox txtInputFileLengthExtractor 
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Enabled         =   0   'False
            Height          =   285
            Left            =   3000
            TabIndex        =   54
            Top             =   840
            Width           =   1215
         End
         Begin CK_Tools.FolderBrowser FolderBrowser1Extractor 
            Height          =   285
            Left            =   120
            TabIndex        =   58
            Top             =   240
            Width           =   4080
            _ExtentX        =   7197
            _ExtentY        =   503
            BackColor       =   14737632
            BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
               Name            =   "MS Sans Serif"
               Size            =   8.25
               Charset         =   0
               Weight          =   400
               Underline       =   0   'False
               Italic          =   0   'False
               Strikethrough   =   0   'False
            EndProperty
         End
         Begin VB.Label Label10 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Input File Name:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   120
            TabIndex        =   60
            Top             =   600
            Width           =   1155
         End
         Begin VB.Label Label11 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "File Length:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   3000
            TabIndex        =   59
            Top             =   600
            Width           =   825
         End
      End
      Begin VB.Frame Frame9 
         BackColor       =   &H00B8B8B8&
         Caption         =   "Options:"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   4455
         Left            =   4560
         TabIndex        =   34
         Top             =   1920
         Width           =   4695
         Begin VB.TextBox txtGENHVersionExtractor 
            Alignment       =   2  'Zentriert
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Height          =   285
            Left            =   360
            Locked          =   -1  'True
            TabIndex        =   43
            Top             =   600
            Width           =   4095
         End
         Begin VB.TextBox txtEmbeddedFileExtractor 
            Alignment       =   2  'Zentriert
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Height          =   285
            Left            =   360
            TabIndex        =   42
            Top             =   1200
            Width           =   4095
         End
         Begin VB.ComboBox txtGENHChannelsExtractor 
            BackColor       =   &H00E0E0E0&
            Height          =   315
            ItemData        =   "Form1.frx":0345
            Left            =   360
            List            =   "Form1.frx":0361
            TabIndex        =   41
            Top             =   3120
            Width           =   1935
         End
         Begin VB.TextBox txtGENHLoopEndSamplesExtractor 
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Height          =   315
            Left            =   2520
            TabIndex        =   40
            Top             =   3960
            Width           =   1935
         End
         Begin VB.TextBox txtGENHLoopStartSamplesExtractor 
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Height          =   315
            Left            =   360
            TabIndex        =   39
            Top             =   3960
            Width           =   1935
         End
         Begin VB.ComboBox txtGENHHeaderSkipExtractor 
            BackColor       =   &H00E0E0E0&
            Height          =   315
            ItemData        =   "Form1.frx":037D
            Left            =   360
            List            =   "Form1.frx":03A8
            TabIndex        =   38
            Top             =   2520
            Width           =   1935
         End
         Begin VB.ComboBox txtGENHInterleaveExtractor 
            BackColor       =   &H00E0E0E0&
            Height          =   315
            ItemData        =   "Form1.frx":03ED
            Left            =   2520
            List            =   "Form1.frx":041E
            TabIndex        =   37
            Top             =   2520
            Width           =   1935
         End
         Begin VB.ComboBox txtGENHFrequencyExtractor 
            BackColor       =   &H00E0E0E0&
            Height          =   315
            ItemData        =   "Form1.frx":0475
            Left            =   2520
            List            =   "Form1.frx":049A
            TabIndex        =   36
            Top             =   3120
            Width           =   1935
         End
         Begin VB.TextBox txtExtractLengthExtractor 
            Alignment       =   2  'Zentriert
            Appearance      =   0  '2D
            BackColor       =   &H00E0E0E0&
            Height          =   285
            Left            =   360
            Locked          =   -1  'True
            TabIndex        =   35
            Top             =   1800
            Width           =   4095
         End
         Begin VB.Label Label19 
            AutoSize        =   -1  'True
            BackStyle       =   0  'Transparent
            Caption         =   "GENH Version:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   52
            Top             =   360
            Width           =   1080
         End
         Begin VB.Label Label20 
            AutoSize        =   -1  'True
            BackStyle       =   0  'Transparent
            Caption         =   "Embedded File:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   51
            Top             =   960
            Width           =   1095
         End
         Begin VB.Label Label21 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Channels:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   50
            Top             =   2880
            Width           =   705
         End
         Begin VB.Label Label22 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Interleave:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   2520
            TabIndex        =   49
            Top             =   2280
            Width           =   750
         End
         Begin VB.Label Label23 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Frequency:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   2520
            TabIndex        =   48
            Top             =   2880
            Width           =   795
         End
         Begin VB.Label Label24 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Loop Start (Samples):"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   47
            Top             =   3720
            Width           =   1515
         End
         Begin VB.Label Label25 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Loop End (Samples):"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   2520
            TabIndex        =   46
            Top             =   3720
            Width           =   1470
         End
         Begin VB.Label Label26 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Header Skip:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   45
            Top             =   2280
            Width           =   930
         End
         Begin VB.Label Label27 
            AutoSize        =   -1  'True
            BackStyle       =   0  'Transparent
            Caption         =   "Extract Length (Bytes):"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   44
            Top             =   1560
            Width           =   1605
         End
      End
      Begin VB.Frame Frame10 
         BackColor       =   &H00B8B8B8&
         Caption         =   "Format:"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   1575
         Left            =   4560
         TabIndex        =   30
         Top             =   240
         Width           =   4695
         Begin VB.ComboBox comboFileFormatExtractor 
            BackColor       =   &H00E0E0E0&
            Height          =   315
            ItemData        =   "Form1.frx":04EA
            Left            =   120
            List            =   "Form1.frx":0512
            TabIndex        =   31
            Top             =   480
            Width           =   4455
         End
         Begin VB.Label lblERRORExtractor 
            Alignment       =   2  'Zentriert
            BackStyle       =   0  'Transparent
            BeginProperty Font 
               Name            =   "MS Sans Serif"
               Size            =   8.25
               Charset         =   0
               Weight          =   700
               Underline       =   0   'False
               Italic          =   0   'False
               Strikethrough   =   0   'False
            EndProperty
            ForeColor       =   &H000000FF&
            Height          =   495
            Left            =   120
            TabIndex        =   33
            Top             =   960
            Width           =   4485
         End
         Begin VB.Label Label28 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "File Format:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   120
            TabIndex        =   32
            Top             =   240
            Width           =   810
         End
      End
   End
   Begin VB.Frame frmCREATOR 
      BackColor       =   &H00B8B8B8&
      Caption         =   "Creator:"
      BeginProperty Font 
         Name            =   "MS Sans Serif"
         Size            =   8.25
         Charset         =   0
         Weight          =   700
         Underline       =   0   'False
         Italic          =   0   'False
         Strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H00800000&
      Height          =   7815
      Left            =   120
      TabIndex        =   3
      Top             =   960
      Width           =   12855
      Begin VB.Frame frmSpecialOptionsPSX 
         BackColor       =   &H00B8B8B8&
         Caption         =   "Special Options (PSX/PS2):"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   1935
         Left            =   9720
         TabIndex        =   120
         Top             =   240
         Visible         =   0   'False
         Width           =   3015
         Begin VB.CommandButton cmdFIndLoopsPSX 
            Caption         =   "Find Loops"
            Height          =   255
            Left            =   120
            TabIndex        =   122
            Top             =   720
            Width           =   2775
         End
         Begin VB.CommandButton cmdFindInterleavePSX 
            Caption         =   "Find Interleave"
            Height          =   255
            Left            =   120
            TabIndex        =   121
            Top             =   360
            Width           =   2775
         End
      End
      Begin VB.CheckBox chkHalfFileInterleave 
         BackColor       =   &H00B8B8B8&
         Caption         =   "Half File Interleave (SPSD only?)"
         ForeColor       =   &H00800000&
         Height          =   255
         Left            =   9720
         TabIndex        =   116
         Top             =   4800
         Visible         =   0   'False
         Width           =   2775
      End
      Begin VB.Frame frmLoopCalculation 
         BackColor       =   &H00B8B8B8&
         Caption         =   "Loop Calculation"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   2775
         Left            =   4560
         TabIndex        =   104
         Top             =   4200
         Width           =   5055
         Begin VB.TextBox txtGENHLoopStartSamples 
            Appearance      =   0  '2D
            BackColor       =   &H00C0C0C0&
            Height          =   315
            Left            =   240
            TabIndex        =   108
            Top             =   480
            Width           =   1455
         End
         Begin VB.TextBox txtGENHLoopEndSamples 
            Appearance      =   0  '2D
            BackColor       =   &H00C0C0C0&
            Height          =   315
            Left            =   240
            TabIndex        =   107
            Top             =   1200
            Width           =   1455
         End
         Begin VB.TextBox txtGENHLoopEnd 
            Appearance      =   0  '2D
            BackColor       =   &H00C0C0C0&
            Height          =   315
            Left            =   1800
            TabIndex        =   106
            Top             =   1200
            Width           =   1455
         End
         Begin VB.TextBox txtGENHLoopStart 
            Appearance      =   0  '2D
            BackColor       =   &H00C0C0C0&
            Height          =   315
            Left            =   1800
            TabIndex        =   105
            Top             =   480
            Width           =   1455
         End
         Begin VB.Label lblUseFileEnd 
            AutoSize        =   -1  'True
            BackStyle       =   0  'Transparent
            Caption         =   "Use File End"
            BeginProperty Font 
               Name            =   "MS Sans Serif"
               Size            =   8.25
               Charset         =   0
               Weight          =   700
               Underline       =   0   'False
               Italic          =   0   'False
               Strikethrough   =   0   'False
            EndProperty
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   2040
            TabIndex        =   115
            Top             =   1920
            Width           =   1095
         End
         Begin VB.Label lblLoopEndToSamples_Click 
            AutoSize        =   -1  'True
            BackStyle       =   0  'Transparent
            Caption         =   "Calculate"
            BeginProperty Font 
               Name            =   "MS Sans Serif"
               Size            =   8.25
               Charset         =   0
               Weight          =   700
               Underline       =   0   'False
               Italic          =   0   'False
               Strikethrough   =   0   'False
            EndProperty
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   3720
            TabIndex        =   114
            Top             =   1280
            Width           =   810
         End
         Begin VB.Label Label5 
            AutoSize        =   -1  'True
            BackStyle       =   0  'Transparent
            Caption         =   "Calculate"
            BeginProperty Font 
               Name            =   "MS Sans Serif"
               Size            =   8.25
               Charset         =   0
               Weight          =   700
               Underline       =   0   'False
               Italic          =   0   'False
               Strikethrough   =   0   'False
            EndProperty
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   3720
            TabIndex        =   113
            Top             =   560
            Width           =   810
         End
         Begin VB.Label Label3 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "Loop End (Bytes):"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   1800
            TabIndex        =   112
            Top             =   960
            Width           =   1260
         End
         Begin VB.Label Label2 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "Loop Start (Bytes):"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   1800
            TabIndex        =   111
            Top             =   240
            Width           =   1305
         End
         Begin VB.Label lblGENHLoopEnd 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "Loop End:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   240
            TabIndex        =   110
            Top             =   960
            Width           =   735
         End
         Begin VB.Label lblGENHLoopStart 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "Loop Start:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   240
            TabIndex        =   109
            Top             =   240
            Width           =   780
         End
         Begin VB.Image cmdUSEFILEEND 
            Height          =   375
            Left            =   480
            Top             =   1800
            Width           =   4095
         End
         Begin VB.Image cmdLoopStartToSamples 
            Height          =   315
            Left            =   3360
            Picture         =   "Form1.frx":068A
            Top             =   480
            Width           =   1455
         End
         Begin VB.Image cmdLoopEndToSamples 
            Height          =   315
            Left            =   3360
            Picture         =   "Form1.frx":1EC0
            Top             =   1200
            Width           =   1455
         End
      End
      Begin VB.CommandButton cmdCreateFormatList 
         Caption         =   "Create format list (dev only)"
         Height          =   255
         Left            =   9960
         TabIndex        =   100
         Top             =   7320
         Visible         =   0   'False
         Width           =   2655
      End
      Begin VB.Frame frmSpecialOptionsGameCube 
         BackColor       =   &H00B8B8B8&
         Caption         =   "Special Options (GameCube):"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   2415
         Left            =   9720
         TabIndex        =   99
         Top             =   240
         Visible         =   0   'False
         Width           =   3015
         Begin VB.TextBox txtDSPCoef1 
            Appearance      =   0  '2D
            BackColor       =   &H00C0C0C0&
            Height          =   285
            Left            =   120
            TabIndex        =   103
            Text            =   "0"
            Top             =   720
            Width           =   1335
         End
         Begin VB.TextBox txtDSPCoef2 
            Appearance      =   0  '2D
            BackColor       =   &H00C0C0C0&
            Height          =   285
            Left            =   1560
            TabIndex        =   102
            Text            =   "0"
            Top             =   720
            Width           =   1335
         End
         Begin VB.CheckBox chkCapcomHack 
            BackColor       =   &H00B8B8B8&
            Caption         =   "Capcom Hack"
            ForeColor       =   &H00800000&
            Height          =   255
            Left            =   120
            TabIndex        =   101
            Top             =   1080
            Width           =   2775
         End
         Begin VB.Label Label31 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Right Channel:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   1560
            TabIndex        =   119
            Top             =   480
            Width           =   1050
         End
         Begin VB.Label Label30 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "Left Channel:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   120
            TabIndex        =   118
            Top             =   480
            Width           =   945
         End
         Begin VB.Label Label29 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            Caption         =   "coefficients offsets:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   120
            TabIndex        =   117
            Top             =   240
            Width           =   1365
         End
      End
      Begin VB.Frame frmINFO 
         Caption         =   "Info and Help:"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   615
         Left            =   120
         TabIndex        =   97
         Top             =   7080
         Width           =   9495
         Begin VB.Label lblINFO 
            BackColor       =   &H00FFFFFF&
            BeginProperty Font 
               Name            =   "MS Sans Serif"
               Size            =   8.25
               Charset         =   0
               Weight          =   700
               Underline       =   0   'False
               Italic          =   0   'False
               Strikethrough   =   0   'False
            EndProperty
            ForeColor       =   &H000000FF&
            Height          =   255
            Left            =   120
            TabIndex        =   98
            Top             =   240
            Width           =   9255
         End
      End
      Begin VB.Frame frmOptionsCreator 
         BackColor       =   &H00B8B8B8&
         Caption         =   "Options:"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   2055
         Left            =   4560
         TabIndex        =   18
         Top             =   2040
         Width           =   5055
         Begin VB.ComboBox txtGENHChannels 
            BackColor       =   &H00C0C0C0&
            Height          =   315
            ItemData        =   "Form1.frx":36F6
            Left            =   360
            List            =   "Form1.frx":3712
            TabIndex        =   22
            Top             =   1080
            Width           =   1935
         End
         Begin VB.ComboBox txtGENHHeaderSkip 
            BackColor       =   &H00C0C0C0&
            Height          =   315
            ItemData        =   "Form1.frx":372E
            Left            =   360
            List            =   "Form1.frx":3759
            TabIndex        =   21
            ToolTipText     =   "Set here the offset where the BGM data starts..."
            Top             =   480
            Width           =   1935
         End
         Begin VB.ComboBox txtGENHInterleave 
            BackColor       =   &H00C0C0C0&
            Height          =   315
            ItemData        =   "Form1.frx":379E
            Left            =   2520
            List            =   "Form1.frx":37CF
            TabIndex        =   20
            Top             =   480
            Width           =   1935
         End
         Begin VB.ComboBox txtGENHFrequency 
            BackColor       =   &H00C0C0C0&
            Height          =   315
            ItemData        =   "Form1.frx":3826
            Left            =   2520
            List            =   "Form1.frx":384B
            TabIndex        =   19
            Top             =   1080
            Width           =   1935
         End
         Begin VB.Label lblGENHChannels 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "Channels:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   26
            Top             =   840
            Width           =   705
         End
         Begin VB.Label lblGENHInterleave 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "Interleave:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   2520
            TabIndex        =   25
            Top             =   240
            Width           =   750
         End
         Begin VB.Label lblGENHFrequency 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "Frequency:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   2520
            TabIndex        =   24
            Top             =   840
            Width           =   795
         End
         Begin VB.Label lblHeaderSkip 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "Header Skip:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   360
            TabIndex        =   23
            Top             =   240
            Width           =   930
         End
      End
      Begin VB.Frame frmFileCreator 
         BackColor       =   &H00B8B8B8&
         Caption         =   "File:"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   6735
         Left            =   120
         TabIndex        =   9
         Top             =   240
         Width           =   4335
         Begin VB.FileListBox File1 
            Appearance      =   0  '2D
            BackColor       =   &H00C0C0C0&
            Height          =   3735
            Left            =   120
            TabIndex        =   28
            Top             =   1800
            Width           =   4095
         End
         Begin VB.TextBox txtInputFileLength 
            Appearance      =   0  '2D
            BackColor       =   &H00C0C0C0&
            Height          =   285
            Left            =   120
            Locked          =   -1  'True
            TabIndex        =   14
            Top             =   1440
            Width           =   1935
         End
         Begin VB.TextBox txtOutputFile 
            Appearance      =   0  '2D
            BackColor       =   &H00C0C0C0&
            Height          =   285
            Left            =   120
            TabIndex        =   13
            Top             =   840
            Width           =   4095
         End
         Begin VB.CheckBox chkCreateOnlyHeader 
            BackColor       =   &H00B8B8B8&
            Caption         =   "Create only Header"
            ForeColor       =   &H00800000&
            Height          =   255
            Left            =   120
            TabIndex        =   12
            Top             =   5880
            Width           =   1695
         End
         Begin VB.CheckBox chkProcessWholeList 
            BackColor       =   &H00B8B8B8&
            Caption         =   "Process Whole List"
            ForeColor       =   &H00800000&
            Height          =   255
            Left            =   120
            TabIndex        =   11
            Top             =   5640
            Width           =   1815
         End
         Begin VB.TextBox txtFilter 
            Alignment       =   2  'Zentriert
            Appearance      =   0  '2D
            BackColor       =   &H00C0C0C0&
            ForeColor       =   &H00800000&
            Height          =   285
            Left            =   2160
            TabIndex        =   10
            Text            =   "*.*"
            Top             =   1440
            Width           =   2055
         End
         Begin CK_Tools.FolderBrowser FolderBrowser1 
            Height          =   285
            Left            =   120
            TabIndex        =   15
            Top             =   240
            Width           =   4080
            _ExtentX        =   7197
            _ExtentY        =   503
            BackColor       =   12632256
            BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
               Name            =   "MS Sans Serif"
               Size            =   8.25
               Charset         =   0
               Weight          =   400
               Underline       =   0   'False
               Italic          =   0   'False
               Strikethrough   =   0   'False
            EndProperty
         End
         Begin VB.Label lblCreateGenh 
            AutoSize        =   -1  'True
            BackStyle       =   0  'Transparent
            Caption         =   "Create Generic Header"
            BeginProperty Font 
               Name            =   "MS Sans Serif"
               Size            =   8.25
               Charset         =   0
               Weight          =   700
               Underline       =   0   'False
               Italic          =   0   'False
               Strikethrough   =   0   'False
            EndProperty
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   1080
            TabIndex        =   94
            Top             =   6345
            Width           =   1965
         End
         Begin VB.Image cmdCreateGENH 
            Height          =   390
            Left            =   120
            Top             =   6240
            Width           =   4095
         End
         Begin VB.Label lblFilterCreator 
            AutoSize        =   -1  'True
            BackStyle       =   0  'Transparent
            Caption         =   "Filter:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   2160
            TabIndex        =   27
            Top             =   1200
            Width           =   375
         End
         Begin VB.Label lblInputFileLength 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "File Length:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   120
            TabIndex        =   17
            Top             =   1200
            Width           =   825
         End
         Begin VB.Label lblOutputFileNameCreator 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "Output File Name:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   120
            TabIndex        =   16
            Top             =   600
            Width           =   1275
         End
      End
      Begin VB.Frame frmFormatCreator 
         BackColor       =   &H00B8B8B8&
         Caption         =   "Format:"
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   8.25
            Charset         =   0
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00800000&
         Height          =   1575
         Left            =   4560
         TabIndex        =   4
         Top             =   240
         Width           =   5055
         Begin VB.ComboBox comboFileFormat 
            BackColor       =   &H00C0C0C0&
            Height          =   315
            ItemData        =   "Form1.frx":389B
            Left            =   120
            List            =   "Form1.frx":38CC
            TabIndex        =   6
            Top             =   480
            Width           =   4815
         End
         Begin VB.ComboBox ComboPresets 
            BackColor       =   &H00C0C0C0&
            Height          =   315
            ItemData        =   "Form1.frx":3AD7
            Left            =   120
            List            =   "Form1.frx":3AE7
            TabIndex        =   5
            Top             =   1080
            Width           =   4815
         End
         Begin VB.Label Label1 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "File Format:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   120
            TabIndex        =   8
            Top             =   240
            Width           =   810
         End
         Begin VB.Label Label4 
            AutoSize        =   -1  'True
            BackColor       =   &H00B8B8B8&
            BackStyle       =   0  'Transparent
            Caption         =   "Presets:"
            ForeColor       =   &H00800000&
            Height          =   195
            Left            =   120
            TabIndex        =   7
            Top             =   840
            Width           =   570
         End
      End
   End
   Begin VB.PictureBox imgEXITNORMAL 
      AutoSize        =   -1  'True
      BorderStyle     =   0  'Kein
      Height          =   285
      Left            =   1800
      Picture         =   "Form1.frx":3BAD
      ScaleHeight     =   285
      ScaleWidth      =   285
      TabIndex        =   2
      Top             =   10680
      Visible         =   0   'False
      Width           =   285
   End
   Begin VB.PictureBox imgEXITOVER 
      AutoSize        =   -1  'True
      BorderStyle     =   0  'Kein
      Height          =   285
      Left            =   2160
      Picture         =   "Form1.frx":416B
      ScaleHeight     =   285
      ScaleWidth      =   285
      TabIndex        =   1
      Top             =   10680
      Visible         =   0   'False
      Width           =   285
   End
   Begin VB.PictureBox imgEXIT 
      BorderStyle     =   0  'Kein
      Height          =   285
      Left            =   1440
      Picture         =   "Form1.frx":4729
      ScaleHeight     =   285
      ScaleWidth      =   285
      TabIndex        =   0
      Top             =   10680
      Width           =   285
   End
   Begin VB.Label lblInputFileNameCreator 
      AutoSize        =   -1  'True
      BackColor       =   &H00B8B8B8&
      BackStyle       =   0  'Transparent
      Caption         =   "Input File Name:"
      ForeColor       =   &H00800000&
      Height          =   195
      Left            =   120
      TabIndex        =   96
      Top             =   10080
      Width           =   1155
   End
   Begin VB.Image imgMINIMIZE 
      Height          =   285
      Left            =   1440
      Picture         =   "Form1.frx":5405
      Top             =   11040
      Width           =   285
   End
   Begin VB.Image imgMINIMIZEOVER 
      Height          =   285
      Left            =   2160
      Picture         =   "Form1.frx":59C3
      Top             =   11040
      Width           =   285
   End
   Begin VB.Image imgMINIMIZENORMAL 
      Height          =   285
      Left            =   1800
      Picture         =   "Form1.frx":5F81
      Top             =   11040
      Width           =   285
   End
   Begin VB.Image btnOVER 
      Height          =   390
      Left            =   8160
      Picture         =   "Form1.frx":653F
      Top             =   10920
      Width           =   4095
   End
   Begin VB.Image btnNormal 
      Height          =   390
      Left            =   8160
      Picture         =   "Form1.frx":B8C9
      Top             =   10560
      Width           =   4095
   End
   Begin VB.Image picBOTTOM 
      Height          =   75
      Left            =   720
      Picture         =   "Form1.frx":10C53
      Stretch         =   -1  'True
      Top             =   11280
      Width           =   15
   End
   Begin VB.Image picRIGHTD 
      Height          =   165
      Left            =   1200
      Picture         =   "Form1.frx":110A9
      Top             =   11280
      Width           =   75
   End
   Begin VB.Image picRIGHTM 
      Height          =   15
      Left            =   1200
      Picture         =   "Form1.frx":11543
      Stretch         =   -1  'True
      Top             =   11160
      Width           =   75
   End
   Begin VB.Image picRIGHTU 
      Height          =   390
      Left            =   1200
      Picture         =   "Form1.frx":1198D
      Top             =   10680
      Width           =   75
   End
   Begin VB.Image picLEFTD 
      Height          =   165
      Left            =   120
      Picture         =   "Form1.frx":11E9F
      Top             =   11280
      Width           =   75
   End
   Begin VB.Image picLEFTM 
      Height          =   15
      Left            =   120
      Picture         =   "Form1.frx":12339
      Stretch         =   -1  'True
      Top             =   11160
      Width           =   75
   End
   Begin VB.Image picLEFTU 
      Height          =   390
      Left            =   120
      Picture         =   "Form1.frx":12783
      Top             =   10680
      Width           =   75
   End
   Begin VB.Image picTOPR 
      Height          =   390
      Left            =   840
      Picture         =   "Form1.frx":12C95
      Top             =   10680
      Width           =   90
   End
   Begin VB.Image picTOPM 
      Height          =   390
      Left            =   720
      Picture         =   "Form1.frx":131A7
      Stretch         =   -1  'True
      Top             =   10680
      Width           =   15
   End
   Begin VB.Image picTOPL 
      Height          =   390
      Left            =   480
      Picture         =   "Form1.frx":13651
      Top             =   10680
      Width           =   90
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
'setting up the skin-feature
Private Declare Function CreateEllipticRgn Lib "gdi32" (ByVal X1 As Long, ByVal Y1 As Long, ByVal X2 As Long, ByVal Y2 As Long) As Long
Private Declare Function CreateRoundRectRgn Lib "gdi32" (ByVal X1 As Long, ByVal Y1 As Long, ByVal X2 As Long, ByVal Y2 As Long, ByVal X3 As Long, ByVal Y3 As Long) As Long
Private Declare Function CombineRgn Lib "gdi32" (ByVal hDestRgn As Long, ByVal hSrcRgn1 As Long, ByVal hSrcRgn2 As Long, ByVal nCombineMode As Long) As Long
Private Declare Function SetWindowRgn Lib "User32" (ByVal hWnd As Long, ByVal hRgn As Long, ByVal bRedraw As Boolean) As Long
Private Declare Function ReleaseCapture Lib "User32" () As Long
Private Declare Function SendMessage Lib "User32" Alias "SendMessageA" (ByVal hWnd As Long, ByVal wMsg As Long, ByVal wParam As Long, lParam As Any) As Long

Private Const WM_NCLBUTTONDOWN = &HA1
Private Const HTCAPTION = 2
Private Const RGN_OR = 2

'For Big Endian reading
Dim START_BYTE As Long

Dim TEMP_BYTE1 As Byte
Dim TEMP_BYTE2 As Byte
Dim TEMP_BYTE3 As Byte
Dim TEMP_BYTE4 As Byte

Dim TEMP_LONG1 As Long
Dim TEMP_LONG2 As Long
Dim TEMP_LONG3 As Long
Dim TEMP_LONG4 As Long

Dim Calculated_Value As Long

'setting up main variables for reading/ exporting, etc
Dim COUNTER
Dim NAMECUT
Dim EXPORTNAME

Dim Samples
Dim PaddingBits

Dim Byte01 As Byte
Dim Byte02 As Byte
Dim Byte03 As Byte
Dim Byte04 As Byte

Dim strByte01 As String * 8
Dim strByte02 As String * 8
Dim strByte03 As String * 8
Dim strByte04 As String * 8
Dim MPEGFRAMEBIN As String * 32

Dim MPEGVersionBIN As String * 2
Dim MPEGLayerBIN As String * 2
Dim MPEGBitRateBIN As String * 4
Dim MPEGSampleRateBIN As String * 2
Dim MPEGPaddingBitBIN As String * 1

Dim MPEGVersion
Dim MPEGLayer
Dim MPEGBitRate
Dim MPEGSampleRate
Dim MPEGPaddingBit
Dim MPEGFrameLength

Dim strInputFile As String
Dim strInputFileEditor As String
Dim strInputFileExtractor As String
Dim strOutputFile As String
Dim strOutputFileExtractor As String

Dim ExportBytesLarge As Long
Dim ExportBytesSmall As Long
Dim SmallBuffer As String * 1
Dim LargeBuffer As String * 2048

Dim END_BYTE As Byte
Dim WholeListCounter

Dim TimeSecondsLoopStart
Dim TimeSecondsCutLoopStart
Dim TimeSecondsLoopEnd
Dim TimeSecondsCutLoopEnd

Dim MSADPCM_Frames
Dim MSADPCM_LastFrame
Dim RiffChannels As Integer
Dim RiffBits As Integer

'setting up needed GENH variables (ordered - all are 4 bytes long)
Dim strGENHCheck As String * 4
Dim strGENH As String * 4               '0x0000
Dim GENHChannels As Long                '0x0004
Dim GENHInterleave As Long              '0x0008
Dim GENHFrequency As Long               '0x000C
Dim GENHLoopStart As Long 'In Samples   '0x0010
Dim GENHLoopEnd As Long 'In Samples     '0x0014
Dim GENHIdentiferByte As Long           '0x0018
Dim GENHHeaderSkip As Long              '0x001C
Dim GENHFileStartOffset As Long         '0x0020
Dim DSPCoef1 As Long                    '0x0024
Dim DSPCoef2 As Long                    '0x0028
Dim DSP_Interleave_Type As Long         '0x002C

Dim strGENHEditor As String * 4               '0x0000
Dim GENHChannelsEditor As Long                '0x0004
Dim GENHInterleaveEditor As Long              '0x0008
Dim GENHFrequencyEditor As Long               '0x000C
Dim GENHLoopStartEditor As Long 'In Samples   '0x0010
Dim GENHLoopEndEditor As Long 'In Samples     '0x0014
Dim GENHIdentiferByteEditor As Long           '0x0018
Dim GENHHeaderSkipEditor As Long              '0x001C
Dim GENHFileStartOffsetEditor As Long         '0x0020
Dim strEmbeddedFileNameEditor As String * 256
Dim strGENHVersionEditorCheck As Byte
Dim strGENHVersionEditor As String * 4
Dim DummyByteEditor As Byte

Dim strGENHCheckExtractor As String * 4
Dim strGENHExtractor As String * 4               '0x0000
Dim GENHChannelsExtractor As Long                '0x0004
Dim GENHInterleaveExtractor As Long              '0x0008
Dim GENHFrequencyExtractor As Long               '0x000C
Dim GENHLoopStartExtractor As Long 'In Samples   '0x0010
Dim GENHLoopEndExtractor As Long 'In Samples     '0x0014
Dim GENHIdentiferByteExtractor As Long           '0x0018
Dim GENHHeaderSkipExtractor As Long              '0x001C
Dim GENHFileStartOffsetExtractor As Long         '0x0020
Dim strEmbeddedFileNameExtractor As String * 256
Dim strGENHVersionExtractorCheck As Byte
Dim strGENHVersionExtractor As String * 4
Dim DummyByteExtractor As Byte
Dim ExportLengthExtractor As Long

'######################## Reserved #####################
'### 'Dim miniGENHNameLength As Long          '0x0024 ##
'### 'Dim GENH_miniGENH_Selector As Long      '0x0028 ##
'######################## Reserved #####################

Dim strGENHVersion As String * 4        '0x0304

Private Sub cmdCreateFormatList_Click()

'nonsense for users, only to retrieve a list of implemented formats
    comboFileFormat.ListIndex = 0
    
    Open "Formats.txt" For Output As #10
    
        Do
        
On Error GoTo EndReached:

    Print #10, comboFileFormat.Text
    
    comboFileFormat.ListIndex = comboFileFormat.ListIndex + 1
    Loop Until comboFileFormat.ListIndex = comboFileFormat.ListCount
    
EndReached:
Close #10
Beep
Exit Sub

End Sub

Private Sub cmdCreateGENH_Click()
    
    DSP_Interleave_Type = 2 '0 = layout_interleave
                            '1 = layout_interleave_byte
                            '2 = layout_none
                            
    DSPCoef1 = 0
    DSPCoef2 = 0
    
    Close #1
    Close #2
    
        '#####################################################
        '#####     Here we're creating the GENH file     #####
        '#####################################################
    
    'Set the "COUNTER" to zero, it's important to prevent
    'freezing if you work on more than one file
    COUNTER = 0
    WholeListCounter = 0
    END_BYTE = &H0
    
    'Set the output file name, given by the textbox
    strOutputFile = txtOutputFile.Text
    

    'Check if we have all basic values we need
    If strInputFile = "" Then
        lblINFO.Caption = ("No File selected!")
        Close #1
        Exit Sub
    ElseIf comboFileFormat.Text = "" Then
        lblINFO.Caption = ("No Format selected!")
        Close #1
        Exit Sub
    ElseIf txtGENHHeaderSkip.Text = "" Then
        lblINFO.Caption = ("Header Size Value not set!")
        Close #1
        Exit Sub
    ElseIf txtGENHChannels.Text = "" Then
        lblINFO.Caption = ("Channel Value not set!")
        Close #1
        Exit Sub
    ElseIf txtGENHFrequency.Text = "" Then
        lblINFO.Caption = ("Frequency Value not set!")
        Close #1
        Exit Sub
    ElseIf txtGENHInterleave.Text = "" Then
        lblINFO.Caption = ("Interleave Value not set!")
        Close #1
        Exit Sub
    End If

    
    GENHInterleave = txtGENHInterleave.Text
    If comboFileFormat.ListIndex = &HB Then
        If GENHInterleave < 1 Then
        lblINFO.Caption = "We need an interleave value to calculate this!"
        Exit Sub
        End If
    End If
    
    'Handling DSP with 1 Channel here
    If comboFileFormat.ListIndex = 12 Then '0x0D GC DSP
        If GENHChannels = 1 Then
            'Check if the textbox has a value
            If txtDSPCoef1.Text = "" Then
                MsgBox ("Start Offset for coef table 1 needed!"), vbInformation, "Generic Header Creator " & strGENHVersion
                Close #1
                Close #2
                Exit Sub
            Else
    'else go on
            DSPCoef1 = txtDSPCoef1.Text
            DSP_Interleave_Type = 2 'layout_none
            End If
        End If
    End If

    'Handling DSP with 2 Channels here
    If comboFileFormat.ListIndex = 12 Then '0x0D GC DSP
        If GENHChannels = 2 Then
            'Check if the textbox has a value
            If txtDSPCoef1.Text = "" Then
                MsgBox ("Start Offset for coef table 1 needed!"), vbInformation, "Generic Header Creator " & strGENHVersion
                Close #1
                Close #2
                Exit Sub
            End If
            If txtDSPCoef2.Text = "" Then
                MsgBox ("Start Offset for coef table 2 needed!"), vbInformation, "Generic Header Creator " & strGENHVersion
                Close #1
                Close #2
                Exit Sub
            Else
            'else go on
            DSPCoef1 = txtDSPCoef1.Text
            DSPCoef2 = txtDSPCoef2.Text
                If GENHInterleave > 7 Then
                    DSP_Interleave_Type = 0 'layout_interleave
                ElseIf GENHInterleave < 8 Then
                    DSP_Interleave_Type = 1 'layout_interleave_byte
                End If
            End If
        End If
    End If


    If GENHInterleave > ((FileLen(strInputFile) - GENHHeaderSkip) / 2) - 4096 Then
        DSP_Interleave_Type = 2 'layout_none
    End If

    
    GENHChannels = txtGENHChannels.Text
    GENHFrequency = txtGENHFrequency.Text
    GENHInterleave = txtGENHInterleave.Text
    
        'Look if we have a Loop End value
        If txtGENHLoopEndSamples.Text = "" Then
            If comboFileFormat.ListIndex = &H8 Then
                lblINFO.Caption = ("Please hit 'Use File End'!")
                Exit Sub
            Else
                Call cmdUSEFILEEND_Click 'If not, Use the File End
            lblINFO.Caption = ("Loop End not set, using the File End!")
            End If
        End If
        
    
    GENHLoopEnd = txtGENHLoopEndSamples.Text
    GENHIdentiferByte = comboFileFormat.ListIndex
    GENHHeaderSkip = txtGENHHeaderSkip.Text

    Open strInputFile For Binary As #1
    Open strOutputFile For Binary As #2

    Put #2, 1, strGENH
    Put #2, 5, GENHChannels
    Put #2, 9, GENHInterleave
    Put #2, 13, GENHFrequency
    
    If txtGENHLoopStartSamples.Text = "" Then
        GENHLoopStart = -1
    Else:
        GENHLoopStart = txtGENHLoopStartSamples.Text
    End If
    
    Put #2, 17, GENHLoopStart
    Put #2, 21, GENHLoopEnd
    Put #2, 25, GENHIdentiferByte
    Put #2, 29, GENHFileStartOffset + GENHHeaderSkip
    Put #2, 33, GENHFileStartOffset

    
    Put #2, 37, DSPCoef1 + GENHFileStartOffset
    Put #2, 41, DSPCoef2 + GENHFileStartOffset

    'For coef which are stored like: 16bytes positive then 16 bytes negative
    If chkCapcomHack.Value = 1 Then
        Put #2, 49, chkCapcomHack.Value
        Put #2, 53, DSPCoef1 + GENHFileStartOffset + 16
        Put #2, 57, DSPCoef2 + GENHFileStartOffset + 16
    ElseIf chkCapcomHack.Value = 0 Then
        Put #2, 49, chkCapcomHack.Value
        Put #2, 53, chkCapcomHack.Value
        Put #2, 57, chkCapcomHack.Value
    End If
    
    
    Put #2, 45, DSP_Interleave_Type
    Put #2, 513, txtGetFileName.Text
    Put #2, 769, (FileLen(strInputFile))
    Put #2, 773, strGENHVersion
        
    'Creating only the Header
    If chkCreateOnlyHeader.Value = 1 Then
        Put #2, 4096, END_BYTE
        Close #1
        Close #2
        Beep
        Exit Sub 'Interrupting the routine here, we'll not go further
    End If

    
    'Calculate the exportbytes, the largebuffer size is variable
    'doing a "ReDim" to match the buffersize in all calculations
    ExportBytesLarge = (Int(FileLen(strInputFile) / 2048) * 2048)
    ExportBytesSmall = (FileLen(strInputFile) - ExportBytesLarge)
    
    
    Do
    
        'Working with a buffer of 2048 bytes, to improve speed
        Get #1, 1 + COUNTER, LargeBuffer
        Put #2, GENHFileStartOffset + 1 + COUNTER, LargeBuffer
    
        'Count until we reached the calculated large buffer size

        COUNTER = COUNTER + 2048
        Loop Until COUNTER = ExportBytesLarge
    
        COUNTER = 0
    
    If ExportBytesSmall > 0 Then

        Get #1, ExportBytesLarge + 1 + COUNTER, SmallBuffer
        Put #2, GENHFileStartOffset + ExportBytesLarge + 1 + COUNTER, SmallBuffer
        
    End If
    
    Close #1
    Close #2
    
    lblINFO.Caption = "Bytes written " & (ExportBytesLarge + ExportBytesSmall) & "/" & (FileLen(strInputFile))
    
    File2.Refresh
    
    
    'Time calculation
    'Loop Start
    If GENHLoopStart = -1 Then
        lblGENHLoopStart.Caption = "Loop Start: 0"
    Else
        txtLoopStartCut.Text = GENHLoopStart / GENHFrequency
        TimeSecondsCutLoopStart = txtLoopStartCut.Text
        lblGENHLoopStart.Caption = "Loop Start: (" & Left(TimeSecondsCutLoopStart, InStrRev(txtLoopStartCut.Text, ",") + 3) & ")"
    End If
    
    'Loop End
    txtLoopEndCut.Text = GENHLoopEnd / GENHFrequency
    TimeSecondsCutLoopEnd = txtLoopEndCut.Text
    lblGENHLoopEnd.Caption = "Loop End: (" & Left(TimeSecondsCutLoopEnd, InStrRev(txtLoopEndCut.Text, ",") + 3) & ")"

    If chkProcessWholeList.Value = 1 Then
        '""
        Else
        Beep
    End If
    
Call PROCESS_WHOLE_LIST

End Sub

Private Sub cmdExtractEmbeddedFile_Click()
    
    COUNTER = 0
    ExportLengthExtractor = txtExtractLengthExtractor.Text
    
    Open strInputFileExtractor For Binary As #5
    Open strOutputFileExtractor For Binary As #6
    
        ExportBytesLarge = (Int(ExportLengthExtractor / 2048) * 2048)
        'Calculating the remaing bytes
        ExportBytesSmall = ExportLengthExtractor - ExportBytesLarge
    
    
    Do
    
        'Working with a buffer of 2048 bytes, to improve speed
        Get #5, 4097 + COUNTER, LargeBuffer
        Put #6, 1 + COUNTER, LargeBuffer
    
        'Count until we reached the calculated large buffer size
        COUNTER = COUNTER + 2048
        Loop Until COUNTER = ExportBytesLarge
        
    
    COUNTER = 0
    
    If ExportBytesSmall > 0 Then
    
        Do
    
            Get #5, ExportBytesLarge + 4097 + COUNTER, SmallBuffer
            Put #6, ExportBytesLarge + 1 + COUNTER, SmallBuffer
            COUNTER = COUNTER + 1
            Loop Until COUNTER = ExportBytesSmall
    
    End If
    
    Close #5
    Close #6
    
    File1.Refresh
    
    Beep
    
End Sub

Private Sub PROCESS_WHOLE_LIST()

    If chkProcessWholeList.Value = 1 Then
        If File1.ListIndex = File1.ListCount - 1 Then
            Close #1
            Close #2
            Beep
            Exit Sub
        Else
            File1.ListIndex = File1.ListIndex + 1
            Call cmdUSEFILEEND_Click
            Call cmdCreateGENH_Click
        End If
    End If
    
End Sub

Private Sub cmdFindInterleavePSX_Click()

    lblINFO.Caption = "Be sure you have set the correct headerskip value...!"

'Check if the needed values are given
    If strInputFile = "" Then
        lblINFO.Caption = ("No File selected!")
        Exit Sub
    End If

Close #1 'Just to be sure, we'll open it later again

COUNTER = 0

    Dim strInterleave1 As String * 16
    Dim strInterleave2 As String * 16
    GENHHeaderSkip = txtGENHHeaderSkip.Text
    
    Open strInputFile For Binary As #1
    Get #1, 1 + GENHHeaderSkip, strInterleave1
    
    Do
        
        Get #1, 17 + GENHHeaderSkip + COUNTER, strInterleave2
        
        If strInterleave1 = strInterleave2 Then
            txtGENHInterleave.Text = COUNTER + 16
            Exit Sub
            Close #1
        End If
        
        COUNTER = COUNTER + 16
    
    Loop Until COUNTER = &H20000
        
    Close #1
    
End Sub

Private Sub cmdFindLoopsPSX_Click()

Close #1
Dim CheckByte As Byte
COUNTER = 0

    If strInputFile = "" Then
        lblINFO.Caption = ("No File selected!")
        Exit Sub
    ElseIf txtGENHChannels.Text = "" Then
        lblINFO.Caption = ("Channel Value not set!")
        Exit Sub
    End If
    
GENHChannels = txtGENHChannels.Text
Open strInputFile For Binary As #1

    Do

    Get #1, 2 + COUNTER, CheckByte
    
    If CheckByte = 0 Then
        lblINFO.Caption = "File has no Loop Flages, using standard values...!"
        txtGENHLoopStartSamples.Text = ""
        cmdUSEFILEEND_Click
        Close #1
        Exit Sub
    End If
    
    If CheckByte = 6 Then
        txtGENHLoopStartSamples.Text = COUNTER / 16 / GENHChannels * 28
        lblINFO.Caption = "Loop Start found (" & txtGENHLoopStartSamples.Text & ")"
    End If
    
    COUNTER = COUNTER + 16
    Loop Until CheckByte = 6

COUNTER = 0
    
    Do

    Get #1, (FileLen(strInputFile) - 14) - COUNTER, CheckByte
    
        If COUNTER = &H20000 Then
            Close #1
            Exit Sub
        End If
        
    If CheckByte = 3 Then
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - COUNTER) / 16 / GENHChannels * 28
        lblINFO.Caption = "Loop End found (" & txtGENHLoopEndSamples.Text & ")"
    End If
    
    COUNTER = COUNTER + 16
    Loop Until CheckByte = 3
    
    
    Close #2
    
    
End Sub

Private Sub cmdUSEFILEEND_Click()
    
    Close #1

'Check if the needed values are given
    If strInputFile = "" Then
        lblINFO.Caption = ("No File selected!")
        Exit Sub
    ElseIf comboFileFormat.Text = "" Then
        lblINFO.Caption = ("Format not set!")
        Exit Sub
    ElseIf txtGENHHeaderSkip.Text = "" Then
        lblINFO.Caption = ("Header Size Value not set!")
        Exit Sub
    ElseIf txtGENHChannels.Text = "" Then
        lblINFO.Caption = ("Channel Value not set!")
        Exit Sub
    End If

'Get the needed variables from the textboxes
GENHHeaderSkip = txtGENHHeaderSkip.Text
GENHChannels = txtGENHChannels.Text

'Depending on the format we'll calculate now the file end (in samples)
Select Case comboFileFormat.ListIndex
    Case &H0 '0x00 - PlayStation 4-bit ADPCM
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / 16 / GENHChannels * 28
    Case &H1 '0x01 - XBOX 4-bit IMA ADPCM
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / 36 / GENHChannels * 64
    Case &H2 '0x02 - GameCube ADP/DTK 4-bit ADPCM
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / 32 * 28
    Case &H3 '0x03 - PCM RAW (Big Endian)
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / 2 / GENHChannels
    Case &H4 '0x04 - PCM RAW (Little Endian)
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / 2 / GENHChannels
    Case &H5 '0x05 - PCM RAW (8-Bit)
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / GENHChannels
    Case &H6 '0x06 - Squareroot-delta-exact 8-bit DPCM
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / 1 / GENHChannels
    Case &H7 '0x07 - Intel DVI 4-Bit IMA ADPCM
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / GENHChannels * 2
    Case &H8 '0x08 - MPEG X Layer X
    
    'Beware, still BETA, need to determine if we really handling with a MPEG frame!!!
    COUNTER = 0
    Samples = 0
    PaddingBits = 0
    
    Open strInputFile For Binary As #1
    'Open "REPORT.TXT" For Output As #2
        
    Do
    
        Get #1, 1 + COUNTER, Byte01
        Get #1, 2 + COUNTER, Byte02
        Get #1, 3 + COUNTER, Byte03
        Get #1, 4 + COUNTER, Byte04
        
    strByte01 = dec2bin(Byte01)
    strByte02 = dec2bin(Byte02)
    strByte03 = dec2bin(Byte03)
    strByte04 = dec2bin(Byte04)
    
    MPEGFRAMEBIN = strByte01 & strByte02 & strByte03 & strByte04

    MPEGVersionBIN = Mid(MPEGFRAMEBIN, 12, 2)
    MPEGLayerBIN = Mid(MPEGFRAMEBIN, 14, 2)
    MPEGBitRateBIN = Mid(MPEGFRAMEBIN, 17, 4)
    MPEGSampleRateBIN = Mid(MPEGFRAMEBIN, 21, 2)
    MPEGPaddingBitBIN = Mid(MPEGFRAMEBIN, 23, 1)
    

    GET_MPEGVERSION
    GET_MPEGLAYER
    GET_MPEGSAMPLERATE
    GET_MPEGPADDINGBIT
    GET_MPEGBITRATE
    

        MPEGFrameLength = (Int((144 * MPEGBitRate / MPEGSampleRate)) + MPEGPaddingBit)
        Samples = Samples + 1
        
        
        'Uncomment it to create a report file
        'Print #2, "Frame at:    " & "0x" & (Hex(COUNTER))
        
        'Print #2, "Byte01 at:   " & "0x" & (Hex(1 + COUNTER)) & " - " & Byte01
        'Print #2, "Byte02 at:   " & "0x" & (Hex(2 + COUNTER)) & " - " & Byte02
        'Print #2, "Byte03 at:   " & "0x" & (Hex(3 + COUNTER)) & " - " & Byte03
        'Print #2, "Byte04 at:   " & "0x" & (Hex(4 + COUNTER)) & " - " & Byte04
        
        'Print #2, "MPEG         " & MPEGVersion & "Layer " & MPEGLayer & " / " & MPEGVersionBIN & "-" & MPEGLayerBIN
        'Print #2, "Sample Rate: " & MPEGSampleRate & " / " & MPEGSampleRateBIN
        'Print #2, "Bit Rate:    " & MPEGBitRate & " / " & MPEGBitRateBIN
        'Print #2, "Padding Bit: " & MPEGPaddingBit & " / " & MPEGPaddingBitBIN
        'Print #2, "" & MPEGFrameLength
        'Print #2, ""
        'Print #2, ""
        
        COUNTER = COUNTER + MPEGFrameLength
        Loop Until COUNTER > (FileLen(strInputFile))
    
    

    If MPEGLayer = 1 Then
        Samples = (Samples - 1) * 384
    ElseIf MPEGLayer = 2 Or MPEGLayer = 3 Then
        Samples = (Samples - 1) * 1152
    End If
    
    
    txtGENHLoopEndSamples.Text = Samples


    Close #1
    'Close #2
    'Beep

    Case &H9 '0x09 - 4-bit IMA ADPCM
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / GENHChannels * 2
    Case &HA '0x0A - Yamaha AICA 4-bit ADPCM
        txtGENHLoopEndSamples.Text = ((FileLen(strInputFile) - GENHHeaderSkip)) / GENHChannels * 2
    
    Case &HB '0x0B - Microsoft 4-bit IMA ADPCM
        If txtGENHInterleave.Text = "" Then
            lblINFO.Caption = "We need an interleave value to calculate this!"
            Exit Sub
        ElseIf txtGENHInterleave.Text < 1 Then
            lblINFO.Caption = "We need an interleave value to calculate this!"
            Exit Sub
        ElseIf txtGENHInterleave.Text > 1 Then
        
        GENHInterleave = txtGENHInterleave.Text
        'Calculating the FrameCount
        MSADPCM_Frames = (FileLen(strInputFile) - GENHHeaderSkip)
        MSADPCM_Frames = (Int(MSADPCM_Frames / GENHInterleave))
        'Look if there a last "short" frame,
        'if yes, calculate the "normal frames" + the short frame
        If (FileLen(strInputFile) - GENHHeaderSkip) - (MSADPCM_Frames * GENHInterleave) > 0 Then
            MSADPCM_LastFrame = (FileLen(strInputFile) - GENHHeaderSkip) - (MSADPCM_Frames * GENHInterleave)
            MSADPCM_Frames = (MSADPCM_Frames * (&H800 - (14 - 2))) + (MSADPCM_LastFrame - (14 - 2))
        Else
        'if not, calculate only the "normal frames"
            MSADPCM_Frames = MSADPCM_Frames * (&H800 - (14 - 2))
        End If
            
            txtGENHLoopEndSamples.Text = MSADPCM_Frames
    End If
    
    
    Case &HC '0x0C - Nintendo GameCube DSP 4-bit ADPCM
                
        If chkCapcomHack.Value = 1 Then
            'Get the Frequency
            Close #1
            START_BYTE = 12
            BIGENDIAN_TO_LITTLEENDIAN
            txtGENHFrequency.Text = Calculated_Value
            'Get the Loop Start
            Close #1
            START_BYTE = 20
            BIGENDIAN_TO_LITTLEENDIAN
                If Calculated_Value = 2 Then
                txtGENHLoopStartSamples.Text = ""
                Else
                txtGENHLoopStartSamples.Text = Calculated_Value / GENHChannels / 8 * 14
                End If
            'Get the Loop End
            Close #1
            START_BYTE = 24
            BIGENDIAN_TO_LITTLEENDIAN
            txtGENHLoopEndSamples.Text = Calculated_Value / GENHChannels / 8 * 14

            txtDSPCoef1.Text = "32"
            txtDSPCoef2.Text = "64"

        ElseIf chkCapcomHack.Value = 0 Then
            txtGENHLoopStartSamples.Text = ""
            txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / GENHChannels / 8 * 14
        End If

    Case &HD '0x0D - PCM RAW (8-Bit Unsigned)
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / GENHChannels
    Case &HE '0x0E - PlayStation 4-bit ADPCM (with bad flags)
        txtGENHLoopEndSamples.Text = (FileLen(strInputFile) - GENHHeaderSkip) / 16 / GENHChannels * 28
    Case comboFileFormat.ListIndex <> comboFileFormat.ListCount
        lblINFO.Caption = ("I guess you haven't selected a format!")
        Exit Sub
    End Select
        
        lblINFO.Caption = ("Calculation done!") 'Guessing it's all OK

End Sub

Private Sub cmdUSEFILEEND_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)

    cmdUSEFILEEND.Picture = btnOVER.Picture
    
End Sub

Private Sub comboFileFormat_Click()

    If comboFileFormat.ListIndex = &H0 Then 'XBOX IMA
        frmSpecialOptionsPSX.Visible = True
    Else
        frmSpecialOptionsPSX.Visible = False
    End If
        
    If comboFileFormat.ListIndex = &H1 Then 'XBOX IMA
        txtGENHInterleave.Text = 0
        txtGENHInterleave.Enabled = False
    ElseIf comboFileFormat.ListIndex = &HD Then 'Unsigned 8-bit PCM
        txtGENHInterleave.Text = 0
        txtGENHInterleave.Enabled = False
    Else
        txtGENHInterleave.Enabled = True
    End If
    
    If comboFileFormat.ListIndex = &HC Then 'NGC DSP
        frmSpecialOptionsGameCube.Visible = True
    Else
        frmSpecialOptionsGameCube.Visible = False
    End If
    
    
End Sub

Private Sub File1_Click()

Close #1 'Just to be sure, to prevent crashes

    If FolderBrowser1.Text = "" Then
        txtInputFile.Text = File1.FileName
    Else
        txtInputFile.Text = FolderBrowser1.Text & "\" & File1.FileName
    End If
    
        strInputFile = txtInputFile.Text
        txtInputFileLength.Text = (FileLen(strInputFile))
        txtOutputFile.Text = txtInputFile.Text & ".GENH"
    
    Open strInputFile For Binary As #1
        
'##############################################################################
'#### Here we'll implement some checks and autogetting for varoius formats ####
'##############################################################################
    '"GENH" - the creator will be disabled if you select a "GENH" file
    Get #1, 1, strGENHCheck
    If strGENHCheck = "GENH" Then
        txtOutputFile.Text = ""
        cmdCreateGENH.Enabled = False
        lblCreateGenh.Enabled = False
        Close #1
        Exit Sub
    Else
    'Else, we'll remove the extension for further things
        cmdCreateGENH.Enabled = True
        lblCreateGenh.Enabled = True
        On Error Resume Next
        NAMECUT = txtInputFile.Text
        txtOutputFile.Text = Left(NAMECUT, InStrRev(txtInputFile.Text, ".") - 1) & ".GENH"
        txtGetFileName.Text = File1.FileName
    End If
    
    
    'SPSD - found in various NAOMI/NAOMI2 and DreamCast games
    If strGENHCheck = "SPSD" Then 'NAOMI / NAOMI 2
        Get #1, 13, GENHLoopEnd
        Get #1, 45, GENHLoopStart
            If GENHLoopStart > GENHLoopEnd Then
                GENHLoopStart = -1
            Else
                GENHLoopStart = GENHLoopStart
            End If
        
        comboFileFormat.ListIndex = 10 'Yamaha ADPCM
        txtGENHHeaderSkip.Text = "64"
        txtGENHLoopEndSamples.Text = GENHLoopEnd
        txtGENHLoopStartSamples.Text = GENHLoopStart
        
        If chkHalfFileInterleave.Value = 1 Then
            GENHInterleave = GENHLoopEnd / 2
        End If
    End If
    
    
    'Riff WAVEfmt - no need to explain this...
    If strGENHCheck = "RIFF" Then
        Get #1, 9, strGENHCheck
        If strGENHCheck = "WAVE" Then
            Get #1, 13, strGENHCheck
            If strGENHCheck = "fmt " Then

                Get #1, 23, RiffChannels
                Get #1, 25, GENHFrequency
                Get #1, 35, RiffBits
                COUNTER = 0
                        
                Do
                        
                Get #1, 1 + COUNTER, strGENHCheck
                If strGENHCheck = "data" Then
                    Get #1, COUNTER + 5, GENHLoopEnd
                    If RiffBits = 8 Then
                        txtGENHLoopEndSamples = GENHLoopEnd / RiffChannels
                        txtGENHHeaderSkip.Text = COUNTER + 8
                        txtGENHChannels.Text = RiffChannels
                        txtGENHFrequency.Text = GENHFrequency
                    ElseIf RiffBits = 16 Then
                        txtGENHLoopEndSamples = GENHLoopEnd / RiffChannels / 2
                        txtGENHHeaderSkip.Text = COUNTER + 8
                        txtGENHChannels.Text = RiffChannels
                        txtGENHFrequency.Text = GENHFrequency
                    End If
                End If
                    COUNTER = COUNTER + 1
                    Loop Until COUNTER = 2048
            End If
        End If
    End If
        
        
        
        
    
    
    Close #1

frmFileCreator.Caption = "File: " & strInputFile
'NO_EXTENSION:
'txtOutputFile.Text = txtInputFile.Text & ".GENH"
'    txtGetFileName.Text = File1.FileName
    
End Sub

Private Sub File1_DblClick()
    
    Open strInputFile For Binary As #1
    Get #1, 1, strGENHCheck
    If strGENHCheck = "GENH" Then
        txtOutputFile.Text = ""
        lblCreateGenh.Enabled = False
        cmdCreateGENH.Enabled = False
        Close #1
        Exit Sub
    Else
    
    cmdUSEFILEEND_Click
    cmdCreateGENH_Click
    
    End If
    
End Sub

Private Sub FolderBrowser1_Change()

    File1.Path = FolderBrowser1.Text

End Sub

Private Sub Form_Load()

    Dim lngRegion1 As Long
    Dim lngRegion2 As Long
    Dim lngRegion3 As Long
    Dim lngRWert As Long

cmdCreateGENH.Picture = btnNormal.Picture
cmdUSEFILEEND = btnNormal.Picture

    picLEFTU.Top = 0
    picLEFTU.Left = 0
        
    picLEFTM.Top = 26
    picLEFTM.Left = 0
    
    picLEFTD.Top = Form1.ScaleHeight - 11
    picLEFTD.Left = 0
    
    
    
    picTOPL.Top = 0
    picTOPM.Top = 0
    picTOPR.Top = 0
    
    picTOPL.Left = 5
    picTOPM.Left = 11
    picTOPR.Left = Form1.ScaleWidth - 11
        
    
    picRIGHTU.Top = 0
    picRIGHTU.Left = Form1.ScaleWidth - 5
    
    picRIGHTM.Left = Form1.ScaleWidth - 5
    picRIGHTM.Top = 26
    
    picRIGHTD.Left = Form1.ScaleWidth - 5
    picRIGHTD.Top = Form1.ScaleHeight - 11
    picRIGHTM.Height = Form1.ScaleHeight - 37
    
    
    picBOTTOM.Top = Form1.ScaleHeight - 6
    picBOTTOM.Left = 5
    picBOTTOM.Width = Form1.ScaleWidth - 10
    
    
    
    picLEFTM.Height = Form1.ScaleHeight - 37
    picTOPM.Width = Form1.ScaleWidth - 22
    
    
    imgEXIT.Left = Form1.ScaleWidth - 27
    imgEXIT.Top = 5
    
    imgMINIMIZE.Left = Form1.ScaleWidth - 47
    imgMINIMIZE.Top = 4
    
    'Regionen erzeugen
    lngRegion1 = CreateRoundRectRgn(Me.ScaleWidth - Me.ScaleWidth / 1, 1, Me.ScaleWidth, Me.ScaleHeight, 20, 20)

    'Regionen kombinieren
    lngRWert = CombineRgn(lngRegion3, lngRegion1, lngRegion2, RGN_OR)

    'Kombinierte Region auf Formular anwenden
    lngRWert = SetWindowRgn(Me.hWnd, lngRegion1, True)

    Label29.BackColor = Form1.BackColor
    Label30.BackColor = Form1.BackColor
    Label31.BackColor = Form1.BackColor
    
    frmCREATOR.BackColor = Form1.BackColor
    frmFileCreator.BackColor = Form1.BackColor
    frmFormatCreator.BackColor = Form1.BackColor
    frmOptionsCreator.BackColor = Form1.BackColor
    frmINFO.BackColor = Form1.BackColor
    frmSpecialOptionsGameCube.BackColor = Form1.BackColor
    frmSpecialOptionsPSX.BackColor = Form1.BackColor
    frmLoopCalculation.BackColor = Form1.BackColor
    
    lblInputFileNameCreator.BackColor = Form1.BackColor
    lblOutputFileNameCreator.BackColor = Form1.BackColor
    lblINFO.BackColor = Form1.BackColor
    chkProcessWholeList.BackColor = Form1.BackColor
    chkCreateOnlyHeader.BackColor = Form1.BackColor
    chkCapcomHack.BackColor = Form1.BackColor
    
    'SSTab1.BackColor = Form1.BackColor
    
    strGENH = "GENH"
    strGENHVersion = "3.00"
    GENHFileStartOffset = 4096
    GENHFileStartOffsetEditor = 4096

    txtGENHHeaderSkip.ListIndex = 0
    txtGENHInterleave.ListIndex = 0
    
    frmCREATOR.Visible = True
    frmEDITOR.Visible = False
    frmExtractor.Visible = False

    cmdSaveEditor.Enabled = False

End Sub

Private Sub Form_MouseDown(Button As Integer, Shift As Integer, X As Single, Y As Single)

    MOVE_FORM
    
End Sub

Private Sub Form_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)

    imgEXIT.Picture = imgEXITNORMAL.Picture
    
End Sub

Private Sub frmCREATOR_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)
    
    imgMINIMIZE.Top = 4
    imgMINIMIZE.Picture = imgMINIMIZENORMAL.Picture
    cmdCreateGENH.Picture = btnNormal.Picture
    
End Sub

Private Sub frmFileCreator_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)

    cmdCreateGENH.Picture = btnNormal.Picture
    
End Sub

Private Sub frmLoopCalculation_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)

    cmdUSEFILEEND.Picture = btnNormal.Picture
    
End Sub

Private Sub frmOptionsCreator_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)

    cmdCreateGENH.Picture = btnNormal.Picture
    cmdUSEFILEEND.Picture = btnNormal.Picture
    
End Sub

Private Sub imgMINIMIZE_Click()

    Form1.WindowState = vbMinimized
    
End Sub

Private Sub imgMINIMIZE_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)

    imgMINIMIZE.Top = 5
    imgMINIMIZE.Picture = imgMINIMIZEOVER.Picture
    imgEXIT.Picture = imgEXITNORMAL.Picture
    
End Sub

Private Sub lblCreateGenh_Click()

    Call cmdCreateGENH_Click
    
End Sub


Private Sub lblLoopEndToSamples_Click_Click()

    Call cmdLoopEndToSamples_Click
    
End Sub

Private Sub lblUseFileEnd_Click()

    Call cmdUSEFILEEND_Click
    
End Sub

Private Sub picRIGHTU_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)
    
    imgMINIMIZE.Top = 4
    imgMINIMIZE.Picture = imgMINIMIZENORMAL.Picture
    imgEXIT.Picture = imgEXITNORMAL.Picture
    
End Sub

Private Sub picTOPR_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)
    
    imgMINIMIZE.Top = 4
    imgMINIMIZE.Picture = imgMINIMIZENORMAL.Picture
    imgEXIT.Picture = imgEXITNORMAL.Picture
    
End Sub

Private Sub picTOPM_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)
    
    imgMINIMIZE.Top = 4
    imgMINIMIZE.Picture = imgMINIMIZENORMAL.Picture
    imgEXIT.Picture = imgEXITNORMAL.Picture
    
End Sub


Private Sub picTOPM_MouseDown(Button As Integer, Shift As Integer, X As Single, Y As Single)
    
    MOVE_FORM

End Sub

Private Sub imgEXIT_Click()

    Unload Me
    
End Sub

Private Sub imgEXIT_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)

    imgMINIMIZE.Top = 4
    imgMINIMIZE.Picture = imgMINIMIZENORMAL.Picture
    imgEXIT.Picture = imgEXITOVER.Picture

End Sub

Private Sub cmdLoopEndToSamples_Click()
    
'We need the file length, file format and the channel count to calculate this...
    If strInputFile = "" Then
        lblINFO.Caption = ("No File selected!")
        Exit Sub 'Exit the complete sub to prevent crashs
    ElseIf comboFileFormat.Text = "" Then
        lblINFO.Caption = ("Format not set!")
        Exit Sub 'Exit the complete sub to prevent crashs
    ElseIf txtGENHChannels.Text = "" Then
        lblINFO.Caption = ("Channel Value not set!")
        Exit Sub 'Exit the complete sub to prevent crashs
    ElseIf txtGENHLoopEnd.Text = "" Then
        lblINFO.Caption = ("Loop End Samples (Bytes) Value not set!")
        Exit Sub 'Exit the complete sub to prevent crashs
    End If
'We now have all needed values and can calculate the samplecount

    GENHChannels = txtGENHChannels.Text

Select Case comboFileFormat.ListIndex
    Case &H0 '0x00 - PlayStation 4-bit ADPCM
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / 16 / GENHChannels * 28
    Case &H1 '0x01 - XBOX 4-bit IMA ADPCM
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / 36 / GENHChannels * 64
    Case &H2 '0x02 - GameCube ADP/DTK 4-bit ADPCM
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / 32 * 28
    Case &H3 '0x03 - PCM RAW (Big Endian)
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / 2 / GENHChannels
    Case &H4 '0x04 - PCM RAW (Little Endian)
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / 2 / GENHChannels
    Case &H5 '0x05 - PCM RAW (8-Bit)
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / GENHChannels
    Case &H6 '0x06 - Squareroot-delta-exact 8-bit DPCM
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / 1 / GENHChannels
    Case &H7 '0x07 - Interleaved DVI 4-Bit IMA ADPCM
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / GENHChannels * 2
    Case &H8 '0x08 - MPEG Layer Audio File (MP1/2/3)
        lblINFO.Caption = ("MPEG can't be calculated like this, sorry!")
    Case &H9 'x09 - 4-bit IMA ADPCM
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / GENHChannels * 2
    Case &HA '0x0A - Yamaha AICA 4-bit ADPCM
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / GENHChannels * 2
    Case &HB '0x0B - Microsoft 4-bit IMA ADPCM
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / GENHChannels * 2
    Case &HC '0x0C - Nintendo GameCube DSP 4-bit ADPCM
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / GENHChannels / 8 * 14
    Case &HD '0x0D - PCM RAW (8-Bit Unsigned)
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / GENHChannels
    Case &HE '0x0E - PlayStation 4-bit ADPCM (with bad flags)
        txtGENHLoopEndSamples.Text = txtGENHLoopEnd.Text / 16 / GENHChannels * 28
    Case comboFileFormat.ListIndex <> comboFileFormat.ListCount
        lblINFO.Caption = ("I guess you haven't selected a format!")
        Exit Sub
End Select
        
        lblINFO.Caption = ("Calculation done!") 'Guessing it's all OK
        
End Sub

Private Sub cmdLoopStartToSamples_Click()
    
'We need the file length, file format and the channel count to calculate this...
    If strInputFile = "" Then
        lblINFO.Caption = ("No File selected!")
        Exit Sub 'Exit the complete sub to prevent crashs
    ElseIf comboFileFormat.Text = "" Then
        lblINFO.Caption = ("Format not set!")
        Exit Sub 'Exit the complete sub to prevent crashs
    ElseIf txtGENHChannels.Text = "" Then
        lblINFO.Caption = ("Channel Value not set!")
        Exit Sub 'Exit the complete sub to prevent crashs
    ElseIf txtGENHLoopStart.Text = "" Then
        lblINFO.Caption = ("Loop Start Samples (Bytes) Value not set!")
        Exit Sub 'Exit the complete sub to prevent crashs
    End If
'We now have all needed values and can calculate the samplecount

    GENHChannels = txtGENHChannels.Text

Select Case comboFileFormat.ListIndex
    Case &H0 '0x00 - PlayStation 4-bit ADPCM
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / 16 / GENHChannels * 28
    Case &H1 '0x01 - XBOX 4-bit IMA ADPCM
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / 36 / GENHChannels * 64
    Case &H2 '0x02 - GameCube ADP/DTK 4-bit ADPCM
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / 32 * 28
    Case &H3 '0x03 - PCM RAW (Big Endian)
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / 2 / GENHChannels
    Case &H4 '0x04 - PCM RAW (Little Endian)
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / 2 / GENHChannels
    Case &H5 '0x05 - PCM RAW (8-Bit)
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / GENHChannels
    Case &H6 '0x06 - Squareroot-delta-exact 8-bit DPCM
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / 1 / GENHChannels
    Case &H7 '0x07 - Interleaved DVI 4-Bit IMA ADPCM
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / GENHChannels * 2
    Case &H8 '0x08 - MPEG Layer Audio File (MP1/2/3)
        lblINFO.Caption = ("MPEG can't be calculated like this, sorry!")
    Case &H9 'x09 - 4-bit IMA ADPCM
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / GENHChannels * 2
    Case &HA '0x0A - Yamaha AICA 4-bit ADPCM
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / GENHChannels * 2
    Case &HB '0x0B - Microsoft 4-bit IMA ADPCM
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / GENHChannels * 2
    Case &HC '0x0C - Nintendo GameCube DSP 4-bit ADPCM
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / GENHChannels / 8 * 14
    Case &HD '0x0D - PCM RAW (8-Bit Unsigned)
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / GENHChannels
    Case &HE '0x0E - PlayStation 4-bit ADPCM (with bad flags)
        txtGENHLoopStartSamples.Text = txtGENHLoopStart.Text / 16 / GENHChannels * 28
    Case comboFileFormat.ListIndex <> comboFileFormat.ListCount
        lblINFO.Caption = ("I guess you haven't selected a format!")
        Exit Sub
End Select
        
        lblINFO.Caption = ("Calculation done!") 'Guessing it's all OK
        
End Sub

Private Sub cmdSaveEditor_Click()
    
    If strInputFileEditor = "" Then
        MsgBox ("No File selected!"), vbInformation, "Generic Header Creator 2"
        Exit Sub
    End If

    
    If comboFileFormatEditor.Text = "" Then
        MsgBox ("No Format selected!"), vbInformation, "Generic Header Creator 2"
        Exit Sub
    End If
    
    If txtGENHHeaderSkipEditor.Text = "" Then
        MsgBox ("Header Size Value not set!"), vbInformation, "Generic Header Creator 2"
        Exit Sub
        Else
        GENHHeaderSkipEditor = txtGENHHeaderSkipEditor.Text
    End If
    
    
    If txtGENHChannelsEditor.Text = "" Then
        MsgBox ("Channel Value not set!"), vbInformation, "Generic Header Creator 2"
        Exit Sub
        Else
        GENHChannelsEditor = txtGENHChannelsEditor.Text
    End If
    
    
    If txtGENHFrequencyEditor.Text = "" Then
        MsgBox ("Frequency Value not set!"), vbInformation, "Generic Header Creator 2"
        Exit Sub
        Else
        GENHFrequencyEditor = txtGENHFrequencyEditor.Text
    End If
    
    
    If txtGENHInterleaveEditor.Text = "" Then
        MsgBox ("Interleave Value not set!"), vbInformation, "Generic Header Creator 2"
        Exit Sub
        Else
        GENHInterleaveEditor = txtGENHInterleaveEditor.Text
    End If
    
    If txtGENHInterleaveEditor.Text = "" Then
        GENHInterleave = 0
        Else
        GENHInterleaveEditor = txtGENHInterleaveEditor.Text
    End If
        
    If txtGENHLoopStartSamplesEditor.Text = "" Then
        GENHLoopStart = -1
        Else
        GENHLoopStartEditor = txtGENHLoopStartSamplesEditor.Text
    End If
    
    If txtGENHLoopEndSamplesEditor.Text = "" Then
        MsgBox ("Loop End Samples Value not set!"), vbInformation, "Generic Header Creator 2"
        Exit Sub
        Else
        GENHLoopEndEditor = txtGENHLoopEndSamplesEditor.Text
    End If

    Call EDIT_GENH

End Sub

Private Sub EDIT_GENH()

    GENHChannelsEditor = txtGENHChannelsEditor.Text
    GENHFrequencyEditor = txtGENHFrequencyEditor.Text
    GENHLoopStartEditor = txtGENHLoopStartSamplesEditor.Text
    GENHLoopEndEditor = txtGENHLoopEndSamplesEditor.Text
    GENHIdentiferByteEditor = comboFileFormatEditor.ListIndex
    GENHHeaderSkipEditor = txtGENHHeaderSkipEditor.Text
    
    strEmbeddedFileNameEditor = txtEmbeddedFileEditor.Text
    Open strInputFileEditor For Binary As #3
    
    
    Put #3, 1, strGENHEditor
    Put #3, 5, GENHChannelsEditor
    Put #3, 9, GENHInterleaveEditor
    Put #3, 13, GENHFrequencyEditor
    Put #3, 17, GENHLoopStartEditor
    Put #3, 21, GENHLoopEndEditor
    Put #3, 25, GENHIdentiferByteEditor
    Put #3, 29, GENHHeaderSkipEditor + 4096
    Put #3, 33, GENHFileStartOffsetEditor
    
    'Clear the 512 byte area which holds the filename
    COUNTER = 0
    DummyByteEditor = 0
    
    Do
    
    Put #3, 513 + COUNTER, DummyByteEditor
    COUNTER = COUNTER + 1
    Loop Until COUNTER = 256
    
    Put #3, 513, txtEmbeddedFileEditor.Text
    
    Put #3, 769, (FileLen(strInputFileEditor)) - 4096
    Put #3, 773, strGENHVersion

Close #3

    Call RECHECK_GENH

End Sub

Private Sub ComboPresets_Click()
    
    If ComboPresets.Text = "PlayStation 4-bit ADPCM - IC=1 - IF=22050" Then
        'PlayStation 4-bit ADPCM - IC=1 - IF=22050
        txtGENHChannels.Text = "1"
        txtGENHFrequency.Text = "22050"
        txtGENHInterleave.Text = "0"
    ElseIf ComboPresets.Text = "PlayStation 4-bit ADPCM - IC=1 - IF=44100" Then
        'PlayStation 4-bit ADPCM - IC=1 - IF=44100
        txtGENHChannels.Text = "1"
        txtGENHFrequency.Text = "44100"
        txtGENHInterleave.Text = "0"
    ElseIf ComboPresets.Text = "PlayStation 4-bit ADPCM - IC=2 - II= 2048 - IF=44100" Then
        'PlayStation 4-bit ADPCM - IC=2 - II= 2048 - IF=44100
        txtGENHChannels.Text = "2"
        txtGENHFrequency.Text = "44100"
        txtGENHInterleave.Text = "2048"
    ElseIf ComboPresets.Text = "PlayStation 4-bit ADPCM - IC=2 - II= 2048 - IF=48000" Then
        'PlayStation 4-bit ADPCM - IC=2 - II= 2048 - IF=48000
        txtGENHChannels.Text = "2"
        txtGENHFrequency.Text = "48000"
        txtGENHInterleave.Text = "2048"
    End If

End Sub


Private Sub File2_Click()

    txtInputFileEditor.Text = File2.FileName
    txtInputFileLengthEditor.Text = (FileLen(File2.FileName))
    strInputFileEditor = txtInputFileEditor.Text
    
    Open strInputFileEditor For Binary As #3
    
    Get #3, 1, strGENHEditor
    If strGENHEditor = "GENH" Then
    cmdSaveEditor.Enabled = True
    lblERROREditor.Caption = ""
    
    Get #3, 5, GENHChannelsEditor
    Get #3, 9, GENHInterleaveEditor
    Get #3, 13, GENHFrequencyEditor
    Get #3, 17, GENHLoopStartEditor
    Get #3, 21, GENHLoopEndEditor
    Get #3, 25, GENHIdentiferByteEditor
    Get #3, 29, GENHHeaderSkipEditor
    Get #3, 33, GENHFileStartOffsetEditor
    Get #3, 773, strGENHVersionEditorCheck
    Get #3, 773, strGENHVersionEditor
    GENHHeaderSkipEditor = GENHHeaderSkipEditor - GENHFileStartOffsetEditor
    
    comboFileFormatEditor.ListIndex = GENHIdentiferByteEditor
    txtGENHChannelsEditor.Text = GENHChannelsEditor
    txtGENHInterleaveEditor.Text = GENHInterleaveEditor
    txtGENHFrequencyEditor.Text = GENHFrequencyEditor
    txtGENHHeaderSkipEditor.Text = GENHHeaderSkipEditor
    txtGENHLoopStartSamplesEditor.Text = GENHLoopStartEditor
    txtGENHLoopEndSamplesEditor.Text = GENHLoopEndEditor
    
    If strGENHVersionEditorCheck = &H0 Then
        txtGENHVersionEditor.Text = "Unknown Build Version"
    Else
        txtGENHVersionEditor.Text = strGENHVersionEditor
    End If
        
    Get #3, 513, strEmbeddedFileNameEditor
        txtEmbeddedFileEditor.Text = strEmbeddedFileNameEditor
    
    
    Else
    cmdSaveEditor.Enabled = False
    lblERROREditor.Caption = "File isn't a valid GENH file!"
    
    
    
    
    End If
    
    Close #3
    
End Sub

Private Sub File3_Click()
    
    txtInputFileExtractor.Text = File3.FileName
    txtInputFileLengthExtractor.Text = (FileLen(File3.FileName))
    strInputFileExtractor = txtInputFileExtractor.Text
    
    Open strInputFileExtractor For Binary As #5
    
    Get #5, 1, strGENHExtractor
    If strGENHExtractor = "GENH" Then
    cmdExtractEmbeddedFile.Enabled = True
    lblERRORExtractor.Caption = ""
    
    Get #5, 5, GENHChannelsExtractor
    Get #5, 9, GENHInterleaveExtractor
    Get #5, 13, GENHFrequencyExtractor
    Get #5, 17, GENHLoopStartExtractor
    Get #5, 21, GENHLoopEndExtractor
    Get #5, 25, GENHIdentiferByteExtractor
    Get #5, 29, GENHHeaderSkipExtractor
    Get #5, 33, GENHFileStartOffsetExtractor
    Get #5, 773, strGENHVersionExtractorCheck
    Get #5, 773, strGENHVersionExtractor
    Get #5, 769, ExportLengthExtractor
    
    GENHHeaderSkipExtractor = GENHHeaderSkipExtractor - GENHFileStartOffsetExtractor
    
    comboFileFormatExtractor.ListIndex = GENHIdentiferByteExtractor
    txtGENHChannelsExtractor.Text = GENHChannelsExtractor
    txtGENHInterleaveExtractor.Text = GENHInterleaveExtractor
    txtGENHFrequencyExtractor.Text = GENHFrequencyExtractor
    txtGENHHeaderSkipExtractor.Text = GENHHeaderSkipExtractor
    txtGENHLoopStartSamplesExtractor.Text = GENHLoopStartExtractor
    txtGENHLoopEndSamplesExtractor.Text = GENHLoopEndExtractor
    txtExtractLengthExtractor.Text = ExportLengthExtractor
    
    If strGENHVersionExtractorCheck = &H0 Then
        txtGENHVersionExtractor.Text = "Unknown Build Version"
    Else
        txtGENHVersionExtractor.Text = strGENHVersionExtractor
    End If
        
    Get #5, 513, strEmbeddedFileNameExtractor
        txtEmbeddedFileExtractor.Text = strEmbeddedFileNameExtractor
        strOutputFileExtractor = txtEmbeddedFileExtractor.Text
    
    Else
    cmdExtractEmbeddedFile.Enabled = False
    lblERRORExtractor.Caption = "File isn't a valid GENH file!"
    
    
    
    
    End If
    
    Close #5
    
End Sub

Private Sub txtFilter_Change()

    File1.Pattern = txtFilter.Text
    
End Sub

Function dec2bin(ByVal n As Long) As String
Do Until n = 0
    If (n Mod 2) Then dec2bin = "1" & dec2bin Else dec2bin = "0" & dec2bin
    n = n \ 2
Loop
End Function

Function GET_MPEGVERSION()
    
    If MPEGVersionBIN = "00" Then 'MPEG V 2.5
        MPEGVersion = "2.5"
    ElseIf MPEGVersionBIN = "10" Then 'MPEG V 2
        MPEGVersion = "2"
    ElseIf MPEGVersionBIN = "11" Then 'MPEG V 1
        MPEGVersion = "1"
    End If
    
End Function

Function GET_MPEGLAYER()
      
    If MPEGLayerBIN = "01" Then 'Layer 3
        MPEGLayer = "3"
    ElseIf MPEGLayerBIN = "10" Then 'Layer 2
        MPEGLayer = "2"
    ElseIf MPEGLayerBIN = "11" Then 'Layer 1
        MPEGLayer = "1"
    End If
    
End Function

Function GET_MPEGSAMPLERATE()

 'MPEG V 2.5
    If MPEGVersionBIN = "00" Then
    If MPEGSampleRateBIN = "00" Then
        MPEGSampleRate = 11025
    ElseIf MPEGSampleRateBIN = "01" Then
        MPEGSampleRate = 12000
    ElseIf MPEGSampleRateBIN = "10" Then
        MPEGSampleRate = 8000
    End If
    End If
    
 
 'MPEG V 1
    If MPEGVersionBIN = "11" Then
    If MPEGSampleRateBIN = "00" Then
        MPEGSampleRate = 44100
    ElseIf MPEGSampleRateBIN = "01" Then
        MPEGSampleRate = 48000
    ElseIf MPEGSampleRateBIN = "10" Then
        MPEGSampleRate = 32000
    End If
    End If
    
    
'MPEG V 2
    If MPEGVersionBIN = "10" Then
    If MPEGSampleRateBIN = "00" Then
        MPEGSampleRate = 22050
    ElseIf MPEGSampleRateBIN = "01" Then
        MPEGSampleRate = 24000
    ElseIf MPEGSampleRateBIN = "10" Then
        MPEGSampleRate = 16000
    End If
    End If
    
End Function

Function GET_MPEGPADDINGBIT()

    If MPEGPaddingBitBIN = "0" Then
        MPEGPaddingBit = "0"
    ElseIf MPEGPaddingBitBIN = "1" Then
        MPEGPaddingBit = "1"
        
        If MPEGLayer = 1 Then   'Layer 1
        PaddingBits = PaddingBits + 4
        ElseIf MPEGLayer = 2 Or MPEGLayer = 3 Then   'Layer 2 & 3
        PaddingBits = PaddingBits + 1
        End If
        
    End If
    
End Function


Function GET_MPEGBITRATE()

    'MPEG 1 Layer 1
    If MPEGVersion = 1 Then 'V1
    If MPEGLayer = 1 Then   'Layer 1
    If MPEGBitRateBIN = "0001" Then
        MPEGBitRate = "32000"
    ElseIf MPEGBitRateBIN = "0010" Then
        MPEGBitRate = "64000"
    ElseIf MPEGBitRateBIN = "0011" Then
        MPEGBitRate = "96000"
    ElseIf MPEGBitRateBIN = "0100" Then
        MPEGBitRate = "128000"
    ElseIf MPEGBitRateBIN = "0101" Then
        MPEGBitRate = "160000"
    ElseIf MPEGBitRateBIN = "0110" Then
        MPEGBitRate = "192000"
    ElseIf MPEGBitRateBIN = "0111" Then
        MPEGBitRate = "224000"
    ElseIf MPEGBitRateBIN = "1000" Then
        MPEGBitRate = "256000"
    ElseIf MPEGBitRateBIN = "1001" Then
        MPEGBitRate = "288000"
    ElseIf MPEGBitRateBIN = "1010" Then
        MPEGBitRate = "320000"
    ElseIf MPEGBitRateBIN = "1011" Then
        MPEGBitRate = "352000"
    ElseIf MPEGBitRateBIN = "1100" Then
        MPEGBitRate = "384000"
    ElseIf MPEGBitRateBIN = "1101" Then
        MPEGBitRate = "416000"
    ElseIf MPEGBitRateBIN = "1110" Then
        MPEGBitRate = "448000"
    End If
    End If
    End If
    
    
    'MPEG 1 Layer 2
    If MPEGVersion = 1 Then 'V1
    If MPEGLayer = 2 Then   'Layer 2
    If MPEGBitRateBIN = "0001" Then
        MPEGBitRate = "32000"
    ElseIf MPEGBitRateBIN = "0010" Then
        MPEGBitRate = "48000"
    ElseIf MPEGBitRateBIN = "0011" Then
        MPEGBitRate = "56000"
    ElseIf MPEGBitRateBIN = "0100" Then
        MPEGBitRate = "64000"
    ElseIf MPEGBitRateBIN = "0101" Then
        MPEGBitRate = "80000"
    ElseIf MPEGBitRateBIN = "0110" Then
        MPEGBitRate = "96000"
    ElseIf MPEGBitRateBIN = "0111" Then
        MPEGBitRate = "112000"
    ElseIf MPEGBitRateBIN = "1000" Then
        MPEGBitRate = "128000"
    ElseIf MPEGBitRateBIN = "1001" Then
        MPEGBitRate = "160000"
    ElseIf MPEGBitRateBIN = "1010" Then
        MPEGBitRate = "192000"
    ElseIf MPEGBitRateBIN = "1011" Then
        MPEGBitRate = "224000"
    ElseIf MPEGBitRateBIN = "1100" Then
        MPEGBitRate = "256000"
    ElseIf MPEGBitRateBIN = "1101" Then
        MPEGBitRate = "320000"
    ElseIf MPEGBitRateBIN = "1110" Then
        MPEGBitRate = "384000"
    End If
    End If
    End If
    
    
    'MPEG 1 Layer 3
    If MPEGVersion = 1 Then 'V1
    If MPEGLayer = 3 Then   'Layer 3
    If MPEGBitRateBIN = "0001" Then
        MPEGBitRate = "32000"
    ElseIf MPEGBitRateBIN = "0010" Then
        MPEGBitRate = "40000"
    ElseIf MPEGBitRateBIN = "0011" Then
        MPEGBitRate = "48000"
    ElseIf MPEGBitRateBIN = "0100" Then
        MPEGBitRate = "56000"
    ElseIf MPEGBitRateBIN = "0101" Then
        MPEGBitRate = "64000"
    ElseIf MPEGBitRateBIN = "0110" Then
        MPEGBitRate = "80000"
    ElseIf MPEGBitRateBIN = "0111" Then
        MPEGBitRate = "96000"
    ElseIf MPEGBitRateBIN = "1000" Then
        MPEGBitRate = "112000"
    ElseIf MPEGBitRateBIN = "1001" Then
        MPEGBitRate = "128000"
    ElseIf MPEGBitRateBIN = "1010" Then
        MPEGBitRate = "160000"
    ElseIf MPEGBitRateBIN = "1011" Then
        MPEGBitRate = "192000"
    ElseIf MPEGBitRateBIN = "1100" Then
        MPEGBitRate = "224000"
    ElseIf MPEGBitRateBIN = "1101" Then
        MPEGBitRate = "256000"
    ElseIf MPEGBitRateBIN = "1110" Then
        MPEGBitRate = "320000"
    End If
    End If
    End If
    
    
    'MPEG 2 Layer 1
    If MPEGVersion = 2 Or MPEGVersion = 2.5 Then 'V2
    If MPEGLayer = 1 Then   'Layer 1
    If MPEGBitRateBIN = "0001" Then
        MPEGBitRate = "32000"
    ElseIf MPEGBitRateBIN = "0010" Then
        MPEGBitRate = "48000"
    ElseIf MPEGBitRateBIN = "0011" Then
        MPEGBitRate = "56000"
    ElseIf MPEGBitRateBIN = "0100" Then
        MPEGBitRate = "64000"
    ElseIf MPEGBitRateBIN = "0101" Then
        MPEGBitRate = "80000"
    ElseIf MPEGBitRateBIN = "0110" Then
        MPEGBitRate = "96000"
    ElseIf MPEGBitRateBIN = "0111" Then
        MPEGBitRate = "112000"
    ElseIf MPEGBitRateBIN = "1000" Then
        MPEGBitRate = "128000"
    ElseIf MPEGBitRateBIN = "1001" Then
        MPEGBitRate = "144000"
    ElseIf MPEGBitRateBIN = "1010" Then
        MPEGBitRate = "160000"
    ElseIf MPEGBitRateBIN = "1011" Then
        MPEGBitRate = "176000"
    ElseIf MPEGBitRateBIN = "1100" Then
        MPEGBitRate = "192000"
    ElseIf MPEGBitRateBIN = "1101" Then
        MPEGBitRate = "224000"
    ElseIf MPEGBitRateBIN = "1110" Then
        MPEGBitRate = "256000"
    End If
    End If
    End If
    
    
    'MPEG 2 & 2.5 Layer 2 & Layer 3
    If MPEGVersion = 2 Or MPEGVersion = 2.5 Then 'V2 & V2.5
    If MPEGLayer = 2 Or MPEGLayer = 3 Then   'Layer 2 & Layer 3
    If MPEGBitRateBIN = "0001" Then
        MPEGBitRate = "8000"
    ElseIf MPEGBitRateBIN = "0010" Then
        MPEGBitRate = "16000"
    ElseIf MPEGBitRateBIN = "0011" Then
        MPEGBitRate = "24000"
    ElseIf MPEGBitRateBIN = "0100" Then
        MPEGBitRate = "32000"
    ElseIf MPEGBitRateBIN = "0101" Then
        MPEGBitRate = "40000"
    ElseIf MPEGBitRateBIN = "0110" Then
        MPEGBitRate = "48000"
    ElseIf MPEGBitRateBIN = "0111" Then
        MPEGBitRate = "56000"
    ElseIf MPEGBitRateBIN = "1000" Then
        MPEGBitRate = "64000"
    ElseIf MPEGBitRateBIN = "1001" Then
        MPEGBitRate = "80000"
    ElseIf MPEGBitRateBIN = "1010" Then
        MPEGBitRate = "96000"
    ElseIf MPEGBitRateBIN = "1011" Then
        MPEGBitRate = "112000"
    ElseIf MPEGBitRateBIN = "1100" Then
        MPEGBitRate = "128000"
    ElseIf MPEGBitRateBIN = "1101" Then
        MPEGBitRate = "144000"
    ElseIf MPEGBitRateBIN = "1110" Then
        MPEGBitRate = "160000"
    End If
    End If
    End If

End Function

Function BIGENDIAN_TO_LITTLEENDIAN()

Open strInputFile For Binary As #1

    Get #1, START_BYTE + 1, TEMP_BYTE1
    Get #1, START_BYTE + 2, TEMP_BYTE2
    Get #1, START_BYTE + 3, TEMP_BYTE3
    Get #1, START_BYTE + 4, TEMP_BYTE4
    
    TEMP_LONG1 = TEMP_BYTE1
    TEMP_LONG2 = TEMP_BYTE2
    TEMP_LONG3 = TEMP_BYTE3
    TEMP_LONG4 = TEMP_BYTE4
    
    TEMP_LONG1 = TEMP_LONG1 * &H1000000
    TEMP_LONG2 = TEMP_LONG2 * &H10000
    TEMP_LONG3 = TEMP_LONG3 * &H100
    TEMP_LONG4 = TEMP_LONG4 * &H1
    
    Calculated_Value = TEMP_LONG1 + TEMP_LONG2 + TEMP_LONG3 + TEMP_LONG4
    
End Function

Private Sub cmdSelectCreator_Click()

    frmCREATOR.Visible = True
    frmEDITOR.Visible = False
    frmExtractor.Visible = False
    
End Sub

Private Sub cmdSelectEditor_Click()
    
    frmCREATOR.Visible = False
    frmEDITOR.Visible = True
    frmExtractor.Visible = False
    
End Sub

Private Sub cmdSelectExtractor_Click()

    frmCREATOR.Visible = False
    frmEDITOR.Visible = False
    frmExtractor.Visible = True
    
End Sub


Private Sub RECHECK_GENH()
    
    Open strInputFileEditor For Binary As #3
    
    Get #3, 5, GENHChannelsEditor
    Get #3, 9, GENHInterleaveEditor
    Get #3, 13, GENHFrequencyEditor
    Get #3, 17, GENHLoopStartEditor
    Get #3, 21, GENHLoopEndEditor
    Get #3, 25, GENHIdentiferByteEditor
    Get #3, 773, strGENHVersionEditorCheck
    Get #3, 773, strGENHVersionEditor
    
    comboFileFormatEditor.ListIndex = GENHIdentiferByteEditor
    txtGENHChannelsEditor.Text = GENHChannelsEditor
    txtGENHInterleaveEditor.Text = GENHInterleaveEditor
    txtGENHFrequencyEditor.Text = GENHFrequencyEditor
    txtGENHHeaderSkipEditor.Text = GENHHeaderSkipEditor
    txtGENHLoopStartSamplesEditor.Text = GENHLoopStartEditor
    txtGENHLoopEndSamplesEditor.Text = GENHLoopEndEditor
    txtGENHVersionEditor.Text = strGENHVersionEditor
    
    Close #3
    
    Beep
    
End Sub

Function MOVE_FORM()
    
    ReleaseCapture
    SendMessage Me.hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0
    
End Function

Private Sub cmdCreateGENH_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)

    cmdCreateGENH.Picture = btnOVER.Picture
    
End Sub

Private Sub chkCreateOnlyHeader_Click()

    cmdCreateGENH.Picture = btnNormal.Picture
    
End Sub


Private Sub chkProcessWholeList_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)

    cmdCreateGENH.Picture = btnNormal.Picture
    
End Sub

Private Sub txtDSPCoef1_KeyPress(KeyAscii As Integer)
  Select Case KeyAscii
    Case Asc("0") To Asc("9"), 8
      'Zeichen zulassen
    Case Else
      KeyAscii = 0 'alles andere verweigern
  End Select
End Sub

Private Sub txtDSPCoef2_KeyPress(KeyAscii As Integer)
  Select Case KeyAscii
    Case Asc("0") To Asc("9"), 8
      'Zeichen zulassen
    Case Else
      KeyAscii = 0 'alles andere verweigern
  End Select
End Sub

Private Sub txtGENHChannels_KeyPress(KeyAscii As Integer)
  Select Case KeyAscii
    Case Asc("1") To Asc("9"), 8
      'Zeichen zulassen
    Case Else
      KeyAscii = 0 'alles andere verweigern
  End Select
End Sub
