VERSION 5.00
Begin VB.Form Form2 
   Caption         =   "Form2"
   ClientHeight    =   3090
   ClientLeft      =   60
   ClientTop       =   450
   ClientWidth     =   6195
   LinkTopic       =   "Form2"
   ScaleHeight     =   3090
   ScaleWidth      =   6195
   StartUpPosition =   3  'Windows-Standard
   Begin VB.CommandButton Command1 
      Caption         =   "Start"
      Height          =   375
      Left            =   1440
      TabIndex        =   1
      Top             =   1560
      Width           =   3135
   End
   Begin VB.TextBox Text1 
      Alignment       =   2  'Zentriert
      Height          =   285
      IMEMode         =   3  'DISABLE
      Left            =   1440
      PasswordChar    =   "*"
      TabIndex        =   0
      Top             =   1200
      Width           =   3135
   End
End
Attribute VB_Name = "Form2"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

Private Sub Command1_Click()

    If Text1.Text = "burnout" Then
        Unload Form2
        Form1.Show
    Else:
        MsgBox ("Wrong password, cya :P")
        Unload Form2
        Unload Form1
    End If
    
        
End Sub
