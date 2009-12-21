unit inputdialog;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, FileUtil, LResources, Forms, Controls, Graphics, Dialogs,
  StdCtrls;

type
  TInputType = (itNumeric, itLiteral);
  TStrCase   = (scBoth, scUpper, scLower);
  TNumRadix  = (nrBinary = 2, nrDecimal = 10, nrHexadecimal = 16);

  { TFormInputDialog }

  TFormInputDialog = class(TForm)
    btnOK:     TButton;
    btnCancel: TButton;
    edtInput:  TEdit;
    lblPrefix: TLabel;
    procedure CenterControl(ctl: TControl; ref: TControl);
    procedure edtInputKeyPress(Sender: TObject; var Key: char);
    procedure FormKeyPress(Sender: TObject; var Key: char);
    procedure FormShow(Sender: TObject);
  private
    { private declarations }
  public
    { public declarations }
    function GetString: string;
    function GetNumber: integer;
    InputType: TInputType;

    CommonMaxLength: integer;
    CommonPrefix: string;
    CommonCase: TStrCase;
    NumMin:   int64;
    NumMax:   int64;
    NumRadix: TNumRadix;
  end;

function StrToIntRadix(sData: string; radix: integer): integer;
function IntToStrRadix(aData, radix: integer): string;

var
  FormInputDialog: TFormInputDialog;

const
  HEX_PARSE_STR: string = '0123456789ABCDEF';
  BYTELEN_ACCORDING_TO_RADIX: array[2..16] of integer =
    (8, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2);

implementation

function IntToStrRadix(aData, radix: integer): string;
var
  t: integer;
begin
  Result := '';
  if (radix = 0) or (radix > 16) then
  begin
    exit;
  end;

  repeat
    t := aData mod radix;
    if t < 10 then
      Result := IntToStr(t) + Result
    else
      Result := InttoHex(t, 1) + Result;
    aData := aData div radix;
  until (aData = 0);
end;

function StrToIntRadix(sData: string; radix: integer): integer;
var
  i, r: integer;
begin
  Result := 0;
  sData  := UpperCase(sData);
  if (radix < 2) or (radix > 16) or (Length(sData) >
    (BYTELEN_ACCORDING_TO_RADIX[radix] * 4)) then
  begin
    exit;
  end;

  for i := 1 to Length(sData) do
  begin
    r := Pos(sData[i], HEX_PARSE_STR);
    if (r < 1) or (r > radix) then
    begin
      exit;
    end;
  end;

  r := 1;
  for i := Length(sData) downto 1 do
  begin
    Inc(Result, r * (Pos(sData[i], HEX_PARSE_STR) - 1));
    r := r * radix;
  end;
end;

{ TFormInputDialog }

procedure TFormInputDialog.CenterControl(ctl: TControl; ref: TControl);
begin
  ctl.Top := ref.Top + (ref.Height - ctl.Height) div 2;
end;

procedure TFormInputDialog.edtInputKeyPress(Sender: TObject; var Key: char);
var
  Value, radix: integer;
  key_upper:    char;
begin
  if Key = char(8) then
  begin
    // backspace is OK
    exit;
  end
  else if CommonCase <> scBoth then
  begin
    if CommonCase = scUpper then
    begin
      Key := AnsiStrUpper(@Key)^;
    end
    else
    begin
      Key := AnsiStrLower(@Key)^;
    end;
  end;

  case InputType of
    itNumeric:
    begin
      // process radix
      radix     := integer(NumRadix);
      key_upper := AnsiStrUpper(@Key)^;
      Value     := Pos(key_upper, HEX_PARSE_STR);
      if (Value > radix) or (Value = 0) then
      begin
        Key := char(0);
        exit;
      end;
      if not ((NumMax = 0) and (NumMin = 0)) then
      begin
        // process Min Max value
      end;
    end;
    itLiteral:
    begin

    end;
  end;
end;

procedure TFormInputDialog.FormKeyPress(Sender: TObject; var Key: char);
begin
  if Key = #27 then
  begin
    Close;
  end;
end;

procedure TFormInputDialog.FormShow(Sender: TObject);
begin
  lblPrefix.Caption  := CommonPrefix;
  edtInput.MaxLength := CommonMaxLength;
  edtInput.Text      := '';

  CenterControl(lblPrefix, edtInput);
  UpdateShowing;
end;

function TFormInputDialog.GetString: string;
begin
  Result := edtInput.Text;
end;

function TFormInputDialog.GetNumber: integer;
var
  str_result: string;
begin
  str_result := edtInput.Text;

  Result := StrToIntRadix(str_result, integer(NumRadix));
end;

initialization
  {$I inputdialog.lrs}

end.

