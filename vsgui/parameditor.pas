unit parameditor;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, FileUtil, LResources, Forms, Controls, Graphics, Dialogs,
  StdCtrls, ExtCtrls;

type
  TParam_Warning = record
    mask: integer;
    value: integer;
    msg: string;
  end;
  TParam_Choice = record
    value: integer;
    text: string;
  end;
  TParam_Setting = record
    name: string;
    info: string;
    mask: integer;
    use_checkbox: boolean;
    use_edit: boolean;
    radix: integer;
    shift: integer;
    checked: integer;
    unchecked: integer;
    enabled: boolean;
    bytelen: integer;
    choices: array of TParam_Choice;
  end;
  TParam_Record = record
    init_value: integer;
    warnings: array of TParam_Warning;
    settings: array of TParam_Setting;
  end;

  { TFormParaEditor }

  TFormParaEditor = class(TForm)
    btnOK: TButton;
    btnCancel: TButton;
    pnlSettings: TPanel;
    pnlButton: TPanel;
    procedure btnOKClick(Sender: TObject);
    procedure FormCloseQuery(Sender: TObject; var CanClose: boolean);
    procedure FormKeyPress(Sender: TObject; var Key: char);
    procedure pnlButtonClick(Sender: TObject);
    procedure pnlSettingsClick(Sender: TObject);
    procedure SettingChange(Sender: TObject);
    procedure FormClose(Sender: TObject; var CloseAction: TCloseAction);
    procedure FormShow(Sender: TObject);
  private
    { private declarations }
    Param_Record: TParam_Record;
    ParaEdtNameArr: array of TEdit;
    ParaComboArr: array of TComboBox;
    ParaCheckArr: array of TCheckBox;
    ParaEdtValueArr: array of TEdit;
    Param_name: string;
    Init_Value, Param_Value, Value_ByteLen: integer;
    EnableWarning: boolean;
    SettingParameter: boolean;
  public
    { public declarations }
    function GetResult(): integer;
    function ParseSettingMaskValue(Sender: TObject; var mask, value: integer): boolean;
    procedure ValueToSetting();
    procedure UpdateTitle();
    procedure SetParameter(init, bytelen, value: integer; title: string; warnEnabled: boolean);
    procedure ParseLine(line: string);
    procedure FreeRecord();
  end;

  function StrToIntRadix(sData: string; radix: integer): integer;
  function IntToStrRadix(aData, radix: Integer): String;
  function GetIntegerParameter(line, para_name: string; var value: integer): boolean;
  function GetStringParameter(line, para_name: string; var value: string): boolean;
  function WipeTailEnter(var line: string): string;

var
  FormParaEditor: TFormParaEditor;
  bCancel: boolean;

const
  EQUAL_STR: string = ' = ';
  LEFT_MARGIN: integer = 10;
  RIGHT_MARGIN: integer = 10;
  TOP_MARGIN: integer = 10;
  BOTTOM_MARGIN: integer = 10;
  X_MARGIN: integer = 4;
  Y_MARGIN: integer = 4;
  ITEM_HEIGHT: integer = 20;
  EDT_WIDTH: integer = 100;
  COMBO_WIDTH: integer = 400;
  SECTION_DIV_HEIGHT: integer = 20;
  HEX_PARSE_STR: string = '0123456789ABCDEF';
  BYTELEN_ACCORDING_TO_RADIX: array[2..16] of integer = (8, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2);

implementation

function IntToStrRadix(aData, radix: Integer): String;
var
  t: Integer;
begin
  Result := '';
  if (radix = 0) or (radix > 16) then
  begin
    exit;
  end;

  repeat
    t := aData mod radix;
    if t < 10 then
      Result := InttoStr(t)+Result
    else
      Result := InttoHex(t, 1)+Result;
    aData := aData div radix;
  until (aData = 0);
end;

function StrToIntRadix(sData: string; radix: integer): integer;
var
  i, r: integer;
begin
  result := 0;
  sData := UpperCase(sData);
  if (radix < 2) or (radix > 16)
     or (Length(sData) > (BYTELEN_ACCORDING_TO_RADIX[radix] * 4)) then
  begin
    exit;
  end;

  for i := 1 to Length(sData) do
  begin
    if (Pos(sData[i], HEX_PARSE_STR) < 1) or (Pos(sData[i], HEX_PARSE_STR) > radix) then
    begin
      exit;
    end;
  end;

  r := 1;
  for i := Length(sData) downto 1 do
  begin
    result := result + r * (Pos(sData[i], HEX_PARSE_STR) - 1);
    r := r * radix;
  end;
end;

function WipeTailEnter(var line: string): string;
begin
  if Pos(#13 + '', line) > 0 then
  begin
    SetLength(line, Length(line) - 1);
  end;
  if Pos(#10 + '', line) > 0 then
  begin
    SetLength(line, Length(line) - 1);
  end;
  result := line;
end;

function GetStringParameter(line, para_name: string; var value: string): boolean;
var
  pos_start, pos_end: integer;
  str_tmp: string;
begin
  WipeTailEnter(line);

  pos_start := Pos(para_name + EQUAL_STR, line);
  if pos_start > 0 then
  begin
    str_tmp := Copy(line, pos_start + Length(para_name + EQUAL_STR), Length(line) - pos_start);

    pos_end := Pos(',', str_tmp);
    if pos_end > 1 then
    begin
      str_tmp := Copy(str_tmp, 1, pos_end - 1);
    end;

    value := str_tmp;
    result := TRUE;
  end
  else
  begin
    value := '';
    result := FALSE;
  end;
end;

function GetIntegerParameter(line, para_name: string; var value: integer): boolean;
var
  pos_start, pos_end: integer;
  str_tmp: string;
begin
  if Pos(#13 + '', line) > 0 then
  begin
    SetLength(line, Length(line) - 1);
  end;

  pos_start := Pos(para_name + EQUAL_STR, line);
  if pos_start > 0 then
  begin
    str_tmp := Copy(line, pos_start + Length(para_name + EQUAL_STR), Length(line) - pos_start);

    pos_end := Pos(',', str_tmp);
    if pos_end > 1 then
    begin
      str_tmp := Copy(str_tmp, 1, pos_end - 1);
    end;

    value := StrToInt(str_tmp);
    result := TRUE;
  end
  else
  begin
    value := 0;
    result := FALSE;
  end;
end;

function GetStringLen_To_ByteLen(bytelen, radix: integer): integer;
begin
  if (radix < 2) or (radix > 16) or (BYTELEN_ACCORDING_TO_RADIX[radix] = 0) then
  begin
    result := 0;
    exit;
  end;

  result := bytelen * BYTELEN_ACCORDING_TO_RADIX[radix];
end;

procedure TFormParaEditor.SettingChange(Sender: TObject);
var
  mask, value: integer;
begin
  if SettingParameter then
  begin
    exit;
  end;

  mask := 0;
  value := 0;
  if ParseSettingMaskValue(Sender, mask, value) = FALSE then
  begin
    exit;
  end;
  Param_Value := Param_Value and not mask;
  Param_Value := Param_Value or value;

  SettingParameter := TRUE;
  ValueToSetting();
  SettingParameter := FALSE;

  UpdateTitle();
end;

procedure TFormParaEditor.FormKeyPress(Sender: TObject; var Key: char);
begin
  if Key = #27 then
  begin
    close;
  end;
end;

procedure TFormParaEditor.pnlButtonClick(Sender: TObject);
begin
  ActiveControl := btnOK;
end;

procedure TFormParaEditor.pnlSettingsClick(Sender: TObject);
begin
  ActiveControl := btnOK;
end;

procedure TFormParaEditor.btnOKClick(Sender: TObject);
begin
  bCancel := FALSE;  
end;

procedure TFormParaEditor.FormCloseQuery(Sender: TObject; var CanClose: boolean
  );
var
  i: integer;
begin
  FormParaEditor.ActiveControl := btnOK;

  CanClose := TRUE;

  if not bCancel and EnableWarning then
  begin
    for i := 0 to Length(Param_Record.warnings) - 1 do
    begin
      if (Param_Value and Param_Record.warnings[i].mask) = Param_Record.warnings[i].value then
      begin
        if mrNo = MessageDlg(Param_Record.warnings[i].msg, mtWarning, [mbYes, mbNo], 0) then
        begin
          bCancel := TRUE;
          CanClose := FALSE;
          exit;
        end
      end;
    end;
  end;
end;

procedure TFormParaEditor.FormClose(Sender: TObject;
  var CloseAction: TCloseAction);
var
  i: integer;
begin
  FreeRecord();

  for i := 0 to Length(ParaEdtNameArr) - 1 do
  begin
    if Assigned(ParaEdtNameArr[i]) then
    begin
      ParaEdtNameArr[i].Destroy;
    end;
  end;
  for i := 0 to Length(ParaComboArr) - 1 do
  begin
    if Assigned(ParaComboArr[i]) then
    begin
      ParaComboArr[i].Destroy;
    end;
  end;
  for i := 0 to Length(ParaCheckArr) - 1 do
  begin
    if Assigned(ParaCheckArr[i]) then
    begin
      ParaCheckArr[i].Destroy;
    end;
  end;
  for i := 0 to Length(ParaEdtValueArr) - 1 do
  begin
    if Assigned(ParaEdtValueArr[i]) then
    begin
      ParaEdtValueArr[i].Destroy;
    end;
  end;

  SetLength(ParaEdtNameArr, 0);
  SetLength(ParaComboArr, 0);
  SetLength(ParaCheckArr, 0);
  SetLength(ParaEdtValueArr, 0);
end;

procedure TFormParaEditor.UpdateTitle();
begin
  Caption := Param_Name + ': 0x' + IntToHex(Param_Value, Value_ByteLen * 2);
end;

function TFormParaEditor.GetResult(): integer;
begin
  result := Param_Value;
end;

procedure TFormParaEditor.SetParameter(init, bytelen, value: integer; title: string; warnEnabled: boolean);
begin
  Init_Value := init;
  Value_ByteLen := bytelen;
  Param_Value := value;
  Param_Name := title;
  EnableWarning := warnEnabled;
end;

function TFormParaEditor.ParseSettingMaskValue(Sender: TObject; var mask, value: integer): boolean;
var
  i: integer;
  str_tmp: string;
begin
  result := TRUE;

  if Sender is TComboBox then
  begin
    i := (Sender as TComboBox).Tag;
  end
  else if Sender is TCheckBox then
  begin
    i := (Sender as TCheckBox).Tag;
  end
  else if Sender is TEdit then
  begin
    i := (Sender as TEdit).Tag;
  end
  else
  begin
    // this component is not supported as a setting component
    result := FALSE;
    exit;
  end;

  mask := Param_Record.settings[i].mask;
  if Param_Record.settings[i].use_edit then
  begin
    str_tmp := Copy(ParaEdtValueArr[i].Text, 1,
            GetStringLen_To_ByteLen(Param_Record.settings[i].bytelen,
                                    Param_Record.settings[i].radix));
    value := StrToIntRadix(str_tmp, Param_Record.settings[i].radix);
    value := value shl Param_Record.settings[i].shift;
    if (value and not mask) > 0 then
    begin
      value := value and mask;
    end;
  end
  else if Param_Record.settings[i].use_checkbox then
  begin
    if ParaCheckArr[i].Checked then
    begin
      value := Param_Record.settings[i].checked;
    end
    else
    begin
      value := Param_Record.settings[i].unchecked;
    end;
  end
  else
  begin
    value := Param_Record.settings[i].choices[ParaComboArr[i].ItemIndex].value;
  end;
end;

procedure TFormParaEditor.ValueToSetting();
var
  i, j: integer;
  value: integer;
  found: boolean;
begin
  SettingParameter := TRUE;
  for i := 0 to Length(Param_Record.settings) - 1 do
  begin
    value := Param_Value and Param_Record.settings[i].mask;
    if Param_Record.settings[i].use_edit then
    begin
      value := value shr Param_Record.settings[i].shift;
      ParaEdtValueArr[i].Text := IntToStrRadix(value, Param_Record.settings[i].radix);
      while Length(ParaEdtValueArr[i].Text)
            < GetStringLen_To_ByteLen(Param_Record.settings[i].bytelen,
                                      Param_Record.settings[i].radix) do
      begin
        ParaEdtValueArr[i].Text := '0'+ParaEdtValueArr[i].Text;
      end;
      ParaEdtValueArr[i].Hint := ParaEdtValueArr[i].Text;
    end
    else if Param_Record.settings[i].use_checkbox then
    begin
      if value = Param_Record.settings[i].checked then
      begin
        ParaCheckArr[i].Checked := TRUE;
      end
      else if value = Param_Record.settings[i].unchecked then
      begin
        ParaCheckArr[i].Checked := FALSE;
      end
      else
      begin
        // unrecognized value
        ParaEdtNameArr[i].Color := clRed;
      end;
    end
    else
    begin
      found := FALSE;
      for j := 0 to Length(Param_Record.settings[i].choices) - 1 do
      begin
        if value = Param_Record.settings[i].choices[j].value then
        begin
          ParaEdtNameArr[i].Color := clWindow;
          ParaComboArr[i].ItemIndex := j;
          found := TRUE;
          break;
        end;
      end;
      if not found then
      begin
        // there is an error
        ParaEdtNameArr[i].Color := clRed;
      end;
      ParaComboArr[i].Hint := ParaComboArr[i].Text;
    end;
  end;
  SettingParameter := FALSE;
end;

procedure TFormParaEditor.FormShow(Sender: TObject);
var
  i, j: integer;
  settings_num, choices_num, section_num, mask: integer;
begin
  bCancel := TRUE;

  // create components according to Param_Record
  settings_num := Length(Param_Record.settings);
  SetLength(ParaEdtNameArr, settings_num);
  SetLength(ParaComboArr, settings_num);
  SetLength(ParaCheckArr, settings_num);
  SetLength(ParaEdtValueArr, settings_num);

  section_num := 0;
  mask := 0;

  SettingParameter := FALSE;
  for i := 0 to settings_num - 1 do
  begin
    if (Param_Record.settings[i].mask and mask) > 0 then
    begin
      section_num := section_num + 1;
      mask := 0;
    end;
    mask := mask or Param_Record.settings[i].mask;

    ParaEdtNameArr[i] := TEdit.Create(Self);
    ParaEdtNameArr[i].Parent := pnlSettings;
    ParaEdtNameArr[i].Top := TOP_MARGIN + i * (Y_MARGIN + ITEM_HEIGHT) + section_num * SECTION_DIV_HEIGHT;
    ParaEdtNameArr[i].Left := LEFT_MARGIN;
    ParaEdtNameArr[i].Width := EDT_WIDTH;
    ParaEdtNameArr[i].Height := ITEM_HEIGHT;
    ParaEdtNameArr[i].Text := Param_Record.settings[i].name;
    if Param_Record.settings[i].use_edit then
    begin
      ParaEdtNameArr[i].Text := ParaEdtNameArr[i].Text + '(r:' + IntToStr(Param_Record.settings[i].radix) + ')';
    end;
    ParaEdtNameArr[i].Color := clWindow;
    ParaEdtNameArr[i].Hint := Param_Record.settings[i].info;
    ParaEdtNameArr[i].ShowHint := TRUE;
    ParaEdtNameArr[i].ReadOnly := TRUE;
    if Param_Record.settings[i].use_edit then
    begin
      ParaEdtValueArr[i] := TEdit.Create(Self);
      ParaEdtValueArr[i].Parent := pnlSettings;
      ParaEdtValueArr[i].Top := TOP_MARGIN + i * (Y_MARGIN + ITEM_HEIGHT) + section_num * SECTION_DIV_HEIGHT;
      ParaEdtValueArr[i].Width := COMBO_WIDTH;
      ParaEdtValueArr[i].Left := LEFT_MARGIN + EDT_WIDTH + X_MARGIN;
      ParaEdtValueArr[i].Height := ITEM_HEIGHT;
      ParaEdtValueArr[i].OnExit := @SettingChange;
      ParaEdtValueArr[i].Tag := i;
      ParaEdtValueArr[i].ShowHint := TRUE;
      ParaEdtValueArr[i].Enabled := Param_Record.settings[i].enabled;
    end
    else if Param_Record.settings[i].use_checkbox then
    begin
      ParaCheckArr[i] := TCheckBox.Create(Self);
      ParaCheckArr[i].Parent := pnlSettings;
      ParaCheckArr[i].Top := TOP_MARGIN + i * (Y_MARGIN + ITEM_HEIGHT) + section_num * SECTION_DIV_HEIGHT;
      ParaCheckArr[i].Left := LEFT_MARGIN + EDT_WIDTH + X_MARGIN;
      ParaCheckArr[i].Height := ITEM_HEIGHT;
      ParaCheckArr[i].OnChange := @SettingChange;
      ParaCheckArr[i].Tag := i;
      ParaCheckArr[i].Enabled := Param_Record.settings[i].enabled;
      ParaCheckArr[i].Caption := '';
      ParaCheckArr[i].Checked := FALSE;
    end
    else
    begin
      ParaComboArr[i] := TComboBox.Create(Self);
      ParaComboArr[i].Parent := pnlSettings;
      ParaComboArr[i].Top := TOP_MARGIN + i * (Y_MARGIN + ITEM_HEIGHT) + section_num * SECTION_DIV_HEIGHT;
      ParaComboArr[i].Left := LEFT_MARGIN + EDT_WIDTH + X_MARGIN;
      ParaComboArr[i].Width := COMBO_WIDTH;
      ParaComboArr[i].Height := ITEM_HEIGHT;
      ParaComboArr[i].OnChange := @SettingChange;
      ParaComboArr[i].Style := csDropDownList;
      ParaComboArr[i].Tag := i;
      ParaComboArr[i].ShowHint := TRUE;
      ParaComboArr[i].Enabled := Param_Record.settings[i].enabled;
      ParaComboArr[i].Clear;
      choices_num := Length(Param_Record.settings[i].choices);
      for j := 0 to choices_num - 1 do
      begin
        ParaComboArr[i].Items.Add(Param_Record.settings[i].choices[j].text);
      end;
    end;
  end;

  i := TOP_MARGIN + BOTTOM_MARGIN + settings_num * (Y_MARGIN + ITEM_HEIGHT) + section_num * SECTION_DIV_HEIGHT;
  ClientHeight := i + pnlButton.Height;
  pnlSettings.ClientHeight := i;
  i := LEFT_MARGIN + RIGHT_MARGIN + EDT_WIDTH + X_MARGIN + COMBO_WIDTH;
  ClientWidth := i;
  pnlSettings.ClientWidth := i;
  pnlButton.ClientWidth := i;
  // center buttons
  btnOK.Left := (pnlButton.Width div 2 - btnOK.Width) div 2;
  btnCancel.Left := pnlButton.Width div 2 + (pnlButton.Width div 2 - btnOK.Width) div 2;
  UpdateTitle();

  ValueToSetting();
  UpdateShowing;
end;

procedure TFormParaEditor.FreeRecord();
begin
  SetLength(Param_Record.warnings, 0);
  SetLength(Param_Record.settings, 0);
end;

procedure TFormParaEditor.ParseLine(line: string);
var
  i, j, num, dis: integer;
begin
  if Pos('warning: ', line) = 1 then
  begin
    i := Length(Param_Record.warnings) + 1;
    SetLength(Param_Record.warnings, i);
    GetIntegerParameter(line, 'mask', Param_Record.warnings[i - 1].mask);
    GetIntegerParameter(line, 'value', Param_Record.warnings[i - 1].value);
    GetStringParameter(line, 'msg', Param_Record.warnings[i - 1].msg);
  end
  else if Pos('setting: ', line) = 1 then
  begin
    i := Length(Param_Record.settings) + 1;
    SetLength(Param_Record.settings, i);
    SetLength(Param_Record.settings[i - 1].choices, 0);
    GetStringParameter(line, 'name', Param_Record.settings[i - 1].name);
    GetIntegerParameter(line, 'mask', Param_Record.settings[i - 1].mask);
    Param_Record.settings[i - 1].bytelen := 0;
    GetIntegerParameter(line, 'bytelen', Param_Record.settings[i - 1].bytelen);
    Param_Record.settings[i - 1].radix := 0;
    GetIntegerParameter(line, 'radix', Param_Record.settings[i - 1].radix);
    if (Param_Record.settings[i - 1].radix < 2) or (Param_Record.settings[i - 1].radix > 16) then
    begin
      Param_Record.settings[i - 1].radix := 10;
    end;
    GetIntegerParameter(line, 'shift', Param_Record.settings[i - 1].shift);
    GetStringParameter(line, 'info', Param_Record.settings[i - 1].info);
    Param_Record.settings[i - 1].use_checkbox := FALSE;
    if GetIntegerParameter(line, 'checked', Param_Record.settings[i - 1].checked) then
    begin
      Param_Record.settings[i - 1].use_checkbox := TRUE;
    end;
    if GetIntegerParameter(line, 'unchecked', Param_Record.settings[i - 1].unchecked) then
    begin
      Param_Record.settings[i - 1].use_checkbox := TRUE;
    end;
    dis := 0;
    GetIntegerParameter(line, 'disabled', dis);
    if dis > 0 then
    begin
      Param_Record.settings[i - 1].enabled := FALSE;
    end
    else
    begin
      Param_Record.settings[i - 1].enabled := TRUE;
    end;
    num := 0;
    GetIntegerParameter(line, 'num_of_choices', num);
    if not Param_Record.settings[i - 1].use_checkbox and (num = 0) then
    begin
      Param_Record.settings[i - 1].use_edit := TRUE;
    end
    else
    begin
      Param_Record.settings[i - 1].use_edit := FALSE;
    end;
  end
  else if Pos('choice: ', line) = 1 then
  begin
    i := Length(Param_Record.settings);
    j := Length(Param_Record.settings[i - 1].choices) + 1;
    SetLength(Param_Record.settings[i - 1].choices, j);
    GetIntegerParameter(line, 'value', Param_Record.settings[i - 1].choices[j - 1].value);
    GetStringParameter(line, 'text', Param_Record.settings[i - 1].choices[j - 1].text);
  end;
end;

initialization
  {$I parameditor.lrs}

end.

