Attribute VB_Name = "Modul1"
Sub ValueChange(Ctrl As Control, ByVal V, ByVal MaxV, ByVal Farbe)
  Vertikal = (Ctrl.ScaleWidth <= Ctrl.ScaleHeight)
  If Vertikal Then
    W = Ctrl.ScaleHeight
    TW = Ctrl.TextWidth("100")
    If Ctrl.Width < TW Then Ctrl.Width = TW
  Else
    W = Ctrl.ScaleWidth
  End If
  Xw = (Abs(V) / Abs(MaxV)) * W
  Proz$ = LTrim$(Str$(Int((Xw / W) * 100)))
  T$ = "%"
  
  F = Farbe
  DM = 14
  Select Case F
  Case 7, 10, 11, 13, 14
    F = 0
    DM = 9
  Case 15
    F = 0
    DM = 6
  End Select
  Ctrl.DrawMode = DM
  Ctrl.ForeColor = QBColor(F)
  
  Ctrl.AutoRedraw = True
  Ctrl.Cls
  
  Select Case Vertikal
  Case 0
    Ctrl.CurrentX = (W - Ctrl.TextWidth(Proz$ + "%")) \ 2
    Ctrl.Print Proz$ + T$;
  Case -1
    Ctrl.CurrentX = (Ctrl.ScaleWidth - Ctrl.TextWidth(Proz$)) \ 2
    Ctrl.CurrentY = (Ctrl.ScaleHeight - 2 * Ctrl.TextHeight(Proz$)) \ 2
    Ctrl.Print Proz$;
    Ctrl.CurrentX = (Ctrl.ScaleWidth - Ctrl.TextWidth("%")) \ 2
    Ctrl.CurrentY = (Ctrl.ScaleHeight - (Ctrl.TextHeight(Proz$)) \ 2) \ 2
    Ctrl.Print "%";
  End Select
  
  If V > 0 Then
    Select Case Vertikal
    Case 0
      Ctrl.Line (0, 0)-(Xw, Ctrl.ScaleHeight), QBColor(Farbe), BF
    Case -1
      Ctrl.Line (0, Ctrl.ScaleHeight - Xw)-(Ctrl.ScaleWidth, Ctrl.ScaleHeight), QBColor(Farbe), BF
    End Select
  End If
  Ctrl.AutoRedraw = False
End Sub


