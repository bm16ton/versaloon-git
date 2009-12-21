unit vsprogtarget;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Contnrs, vsprogparser;

type

  { TTargetArea }

  TTargetArea = class(TObject)
  private
    FName:      char;
    FSegAddr:   cardinal;
    FStartAddr: cardinal;
    FByteLen:   integer;
    FDefaultValue: cardinal;
    FInFile:    boolean;
  public
    constructor Create(aName: char);
    property Name: char Read FName;
    property InFile: boolean Read FInFile;
    property SegAddr: cardinal Read FSegAddr;
    property StartAddr: cardinal Read FStartAddr;
    property ByteLen: integer Read FByteLen;
    property DefaultValue: cardinal Read FDefaultValue;
  end;

  { TTargetChip }

  TTargetChip = class(TObjectList)
  private
    FName:  string;
    FID:    cardinal;
    FMode:  string;      // eg: 'ijh'
    FAreas: string;      // eg: 'f1e1l0u0' -- Flash(File)/EEPROM(File)/Lock/Fuse
    FExtraStr: string;   // eg: For COM setting
    function GetTargetAreas(Index: integer): TTargetArea;
    function GetModeCount: integer;
    function GetModeByIdx(Index: integer): string;
  public
    constructor Create(aName: string);
    function TargetAreaParser(var line: string): boolean;
    function HasArea(aArea: char): boolean;
    function GetArea(aArea: char): TTargetArea;
    function GetAreaIdx(aArea: char): integer;
    property TargetAreas[Index: integer]: TTargetArea Read GetTargetAreas; default;
    property Name: string Read FName;
    property ID: cardinal Read FID;
    property Mode: string Read FMode;
    property Areas: string Read FAreas;
    property ModeCount: integer Read GetModeCount;
    property AreaCount: integer Read GetCount;
  end;

  { TFakeArea }

  TFakeArea = class(TObject)
  private
    FName: Char;
    FFakeSeg: integer;
    FFakeSegEn: boolean;
    FFakeAddr: integer;
    FFakeAddrEn: boolean;
  public
    constructor Create(aName: Char);
    property Name: Char Read FName;
    property FakeSeg: integer Read FFakeSeg;
    property FakeSegEn: boolean Read FFakeSegEn;
    property FakeAddr: integer Read FFakeAddr;
    property FakeAddrEn: boolean Read FFakeAddrEn;
  end;

  { TFakeAreaList }

  TFakeAreaList = class(TObjectList)
  private
    function GetFakeAreas(Index: integer): TFakeArea;
  public
    constructor Create;
    function GetFakeArea(aName: Char): TFakeArea;
    function ParseFake(var line: string): boolean;
    property FakeCount: integer Read GetCount;
    property FakeAreas[Index: integer]: TFakeArea Read GetFakeAreas; default;
  end;

  { TTargetSeries }

  TTargetSeries = class(TObjectList)
  private
    FName:    string;
    FSeriesInfo: string;
    FFeature: string; // eg: X for execute; C for COM;
    FMode:    string; // eg: 'i:ISP(F)|j:JTAG(F)|h:HVPP|s:HVSP'
    FFakeAreas: TFakeAreaList;
    function bAutoDetect: boolean;
    function bNeedComm: boolean;
    function bExecute: boolean;
    function GetTargetChips(Index: integer): TTargetChip;
    function GetModeByModeChar(aModeChar: Char): string;
    function GetFakeCount: integer;
    function GetFakeAreas(Index: integer): TFakeArea;
  public
    constructor Create(aName: string; aFeature: string; aInfo: string);
    destructor Destroy;
    function GetModeStrByModeChar(aModeChar: Char): string;
    function GetModeFeatureByModeChar(aModeChar: Char): string;
    property TargetChips[Index: integer]: TTargetChip Read GetTargetChips; default;
    property Name: string Read FName;
    property AutoDetect: boolean Read bAutoDetect;
    property NeedComm: boolean Read bNeedComm;
    property CanExecute: boolean Read bExecute;
    property Feature: string Read FFeature;
    property SeriesInfo: string Read FSeriesInfo;
    property Mode: string Read FMode;
    property ChipCount: integer Read GetCount;
    property FakeCount: integer Read GetFakeCount;
    function GetFakeArea(aName: Char): TFakeArea;
    property FakeAreas[Index: integer]: TFakeArea Read GetFakeAreas;
  end;

  { TVSProg_Targets }

  TVSProg_Targets = class(TObjectList)
  private
    function GetTargetSeries(Index: integer): TTargetSeries;
  public
    constructor Create;
    function TargetParser(var line: string): boolean;
    property TargetSeries[Index: integer]: TTargetSeries Read GetTargetSeries; default;
    property SeriesCount: integer Read GetCount;
  end;

  TTargetAreaName = record
    ShortName: char;
    FullName:  string;
  end;

function GetAreaFullName(aShortName: char): string;
function GetAreaShortName(aFullName: string): char;

const
  SUPPORT_LIST_INDICATOR: string = 'Support list of ';
  TARGET_AREA_STRING: array [0 .. 5] of TTargetAreaName =
    (
    (ShortName: 'f'; FullName: 'flash'),
    (ShortName: 'e'; FullName: 'eeprom'),
    (ShortName: 'u'; FullName: 'fuse'),
    (ShortName: 'l'; FullName: 'lock'),
    (ShortName: 'c'; FullName: 'cali'),
    (ShortName: 's'; FullName: 'usrsig')
    );

  FLASH_CHAR: char  = 'f';
  EE_CHAR: char     = 'e';
  FUSE_CHAR: char   = 'u';
  LOCK_CHAR: char   = 'l';
  CALI_CHAR: char   = 'c';
  USRSIG_CHAR: char = 's';

implementation

function GetAreaShortName(aFullName: string): char;
var
  i: integer;
begin
  Result := char(0);
  for i := low(TARGET_AREA_STRING) to high(TARGET_AREA_STRING) do
  begin
    if TARGET_AREA_STRING[i].FullName = aFullName then
    begin
      Result := TARGET_AREA_STRING[i].ShortName;
      break;
    end;
  end;
end;

function GetAreaFullName(aShortName: char): string;
var
  i: integer;
begin
  Result := '';
  for i := low(TARGET_AREA_STRING) to high(TARGET_AREA_STRING) do
  begin
    if TARGET_AREA_STRING[i].ShortName = aShortName then
    begin
      Result := TARGET_AREA_STRING[i].FullName;
      break;
    end;
  end;
end;

{ TTargetArea }
constructor TTargetArea.Create(aName: char);
begin
  inherited Create;
  FName := aName;
end;

{ TTargetChip }
constructor TTargetChip.Create(aName: string);
begin
  inherited Create(True);
  FName := aName;
end;

function TTargetChip.TargetAreaParser(var line: string): boolean;
var
  i: integer;
  AreaChar: Char;
  CurValid: boolean;
  CurArea: TTargetArea;
  tmpArea: TTargetArea;
begin
  Result := True;
  for i := 0 to (Length(FAreas) div 2) - 1 do
  begin
    // parse target area FArea[i * 2]
    AreaChar := FAreas[1 + i * 2];
    CurValid := False;
    CurArea := TTargetArea.Create(AreaChar);
    CurArea.FStartAddr := 0;
    CurArea.FSegAddr := 0;
    CurArea.FByteLen := 0;
    CurArea.FDefaultValue := 0;

    if FAreas[2 + i * 2] = '1' then
    begin
      CurValid := GetNumericParameter(line, AreaChar + '_addr', CurArea.FStartAddr) or CurValid;
      CurValid := GetNumericParameter(line, AreaChar + '_seg', CurArea.FSegAddr) or CurValid;
      CurArea.FInFile := True;
    end
    else
    begin
      CurArea.FInFile := False;
    end;
    CurValid := GetNumericParameter(line, AreaChar + '_bytelen', CurArea.FByteLen) or CurValid;
    CurValid := GetNumericParameter(line, AreaChar + '_default', CurArea.FDefaultValue) or CurValid;
    if CurValid then
    begin
      tmpArea := GetArea(AreaChar);
      if tmpArea = nil then
      begin
        Add(CurArea);
      end
      else
      begin
        tmpArea.FStartAddr := CurArea.StartAddr;
        tmpArea.FSegAddr   := CurArea.SegAddr;
        tmpArea.FByteLen   := CurArea.ByteLen;
        tmpArea.FDefaultValue := CurArea.DefaultValue;
        CurArea.Destroy;
      end;
    end
    else
    begin
      CurArea.Destroy;
    end;
  end;
end;

function TTargetChip.GetTargetAreas(Index: integer): TTargetArea;
begin
  Result := TTargetArea(inherited Get(Index));
end;

function TTargetChip.HasArea(aArea: char): boolean;
var
  i: integer;
begin
  Result := False;
  if Pos(aArea, FAreas) > 0 then
  begin
    for i := 0 to AreaCount - 1 do
    begin
      if TargetAreas[i].Name = aArea then
      begin
        Result := True;
        break;
      end;
    end;
  end;
end;

function TTargetChip.GetArea(aArea: char): TTargetArea;
begin
  Result := nil;
  if HasArea(aArea) then
  begin
    Result := TargetAreas[GetAreaIdx(aArea)];
  end;
end;

function TTargetChip.GetAreaIdx(aArea: char): integer;
var
  i: integer;
begin
  Result := -1;
  if Pos(aArea, FAreas) > 0 then
  begin
    for i := 0 to AreaCount - 1 do
    begin
      if TargetAreas[i].Name = aArea then
      begin
        Result := i;
        break;
      end;
    end;
  end;
end;

function TTargetChip.GetModeCount: integer;
var
  strTmp: string;
begin
  Result := 0;
  repeat
    strTmp := GetModeByIdx(Result);
    Inc(Result);
  until strTmp = '';
  Dec(Result);
end;

function TTargetChip.GetModeByIdx(Index: integer): string;
var
  start_pos: integer;
begin
  Result := FMode + '|';
  while Index > 0 do
  begin
    start_pos := Pos('|', Result);
    if start_pos > 0 then
    begin
      Result := Copy(Result, Pos('|', Result) + 1, Length(Result));
    end;
    Dec(Index);
  end;
  start_pos := Pos('|', Result);
  if start_pos > 0 then
  begin
    Result := Copy(Result, 1, start_pos - 1);
  end;
end;

{ TFakeArea }

constructor TFakeArea.Create(aName: Char);
begin
  inherited Create;
  FName := aName;
end;

{ TFakeAreaList }

constructor TFakeAreaList.Create;
begin
  inherited Create(True);
end;

function TFakeAreaList.GetFakeAreas(Index: integer): TFakeArea;
begin
  Result := TFakeArea(inherited Get(Index));
end;

function TFakeAreaList.GetFakeArea(aName: Char): TFakeArea;
var
  i: integer;
begin
  Result := nil;
  for i := 0 to FakeCount - 1 do
  begin
    if FakeAreas[i].Name = aName then
    begin
      Result := FakeAreas[i];
    end;
  end;
end;

function TFakeAreaList.ParseFake(var line: string): boolean;
var
  i: integer;
  AreaChar: Char;
  CurValid, tmpBool: boolean;
  CurFakeArea: TFakeArea;
  tmpFakeArea: TFakeArea;
begin
  Result := True;
  for i := low(TARGET_AREA_STRING) to high(TARGET_AREA_STRING) do
  begin
    AreaChar := TARGET_AREA_STRING[i].ShortName;
    CurValid := False;
    CurFakeArea := TFakeArea.Create(AreaChar);
    CurFakeArea.FFakeAddr := 0;
    CurFakeArea.FFakeAddrEn := False;
    CurFakeArea.FFakeSeg := 0;
    CurFakeArea.FFakeSegEn := False;

    tmpBool  := GetNumericParameter(line, AreaChar + '_faddr', CurFakeArea.FFakeAddr);
    if tmpBool then
    begin
      CurFakeArea.FFakeAddrEn := True;
      CurValid := True;
    end;
    tmpBool  := GetNumericParameter(line, AreaChar + '_fseg', CurFakeArea.FFakeSeg);
    if tmpBool then
    begin
      CurFakeArea.FFakeSegEn := True;
      CurValid := True;
    end;

    if CurValid then
    begin
      tmpFakeArea := GetFakeArea(AreaChar);
      if tmpFakeArea = nil then
      begin
        Add(CurFakeArea);
      end
      else
      begin
        tmpFakeArea.FFakeAddr   := CurFakeArea.FFakeAddr;
        tmpFakeArea.FFakeAddrEn := CurFakeArea.FFakeAddrEn;
        tmpFakeArea.FFakeSeg    := CurFakeArea.FFakeSeg;
        tmpFakeArea.FFakeSegEn  := CurFakeArea.FFakeSegEn;
        CurFakeArea.Destroy;
      end;
    end
    else
    begin
      CurFakeArea.Destroy;
    end;
  end;
end;

{ TTargetSeries }

constructor TTargetSeries.Create(aName: string; aFeature: string; aInfo: string);
begin
  inherited Create(True);
  FFakeAreas := TFakeAreaList.Create;
  FName    := aName;
  FFeature := aFeature;
  FSeriesInfo := aInfo;
end;

destructor TTargetSeries.Destroy;
begin
  inherited Destroy;
  FFakeAreas.Destroy;
end;

function TTargetSeries.GetFakeArea(aName: Char): TFakeArea;
begin
  Result := FFakeAreas.GetFakeArea(aName);
end;

function TTargetSeries.GetFakeCount: integer;
begin
  Result := FFakeAreas.Count;
end;

function TTargetSeries.GetFakeAreas(Index: integer): TFakeArea;
begin
  Result := FFakeAreas.FakeAreas[Index];
end;

function TTargetSeries.GetModeByModeChar(aModeChar: Char): string;
var
  start_pos: integer;
begin
  Result := '';
  start_pos := Pos(aModeChar + ':', FMode);
  if start_pos > 0 then
  begin
    Result := Copy(FMode, start_pos, Length(FMode));
    start_pos := Pos('|', Result);
    if start_pos > 0 then
    begin
      Result := Copy(Result, 1, start_pos - 1);
    end;
  end;
end;

function TTargetSeries.GetModeFeatureByModeChar(aModeChar: Char): string;
var
  posStart: integer;
begin
  Result := GetModeByModeChar(aModeChar);
  if Result <> '' then
  begin
    posStart := Pos('(', Result);
    if posStart > 0 then
    begin
      Result := Copy(Result, posStart + 1, Length(Result) - posStart - 1);
    end;
  end;
end;

function TTargetSeries.GetModeStrByModeChar(aModeChar: Char): string;
var
  posStart: integer;
begin
  Result := GetModeByModeChar(aModeChar);
  if Result <> '' then
  begin
    posStart := Pos('(', Result);
    if posStart > 0 then
    begin
      Result := Copy(Result, 1, posStart - 1);
    end;
  end;
end;

function TTargetSeries.GetTargetChips(Index: integer): TTargetChip;
begin
  Result := TTargetChip(inherited Get(Index));
end;

function TTargetSeries.bAutoDetect: boolean;
begin
  Result := (ChipCount > 0) and (TargetChips[0].Name = FName) and (Pos('A', FFeature) > 0);
end;

function TTargetSeries.bNeedComm: boolean;
begin
  Result := Pos('C', FFeature) > 0;
end;

function TTargetSeries.bExecute: boolean;
begin
  Result := Pos('X', FFeature) > 0;
end;

{ TVSProg_Targets }

constructor TVSProg_Targets.Create;
begin
  inherited Create(True);
end;

function TVSProg_Targets.GetTargetSeries(Index: integer): TTargetSeries;
begin
  Result := TTargetSeries(inherited Get(Index));
end;

function TVSProg_Targets.TargetParser(var line: string): boolean;
const
  CurTarget: string  = '';
  CurFeature: string = '';
var
  strTmp: string;
  start_pos: integer;
  end_pos: integer;
  i: integer;
begin
  Result := True;
  if line = '' then
  begin
    // Ignore Empty Lines
  end
  else if Pos(SUPPORT_LIST_INDICATOR, line) = 1 then
  begin
    // Parse TargetSeries
    start_pos := Length(SUPPORT_LIST_INDICATOR);
    end_pos   := Pos('(', line);
    if end_pos > 0 then
    begin
      // Format 'Name(Feature):SeriesInfo'
      CurTarget  := Copy(line, start_pos + 1, end_pos - start_pos - 1);
      CurFeature := Copy(line, end_pos + 1, Pos(')', line) - end_pos - 1);
      strTmp     := Copy(line, Pos(':', line) + 1, Length(line));
    end
    else
    begin
      // Format 'Name:'
      end_pos    := Pos(':', line);
      CurTarget  := Copy(line, start_pos + 1, end_pos - start_pos - 1);
      CurFeature := '';
      strTmp     := Copy(line, Pos(':', line) + 1, Length(line));
    end;

    Add(TTargetSeries.Create(CurTarget, CurFeature, strTmp));
    TargetSeries[SeriesCount - 1].FFakeAreas.ParseFake(strTmp);
  end
  else if CurTarget <> '' then
  begin
    // Parse TargetChip
    with TargetSeries[SeriesCount - 1] do
    begin
      // Name
      end_pos := Pos(':', line);
      if (end_pos > 0) then
      begin
        strTmp := Copy(line, 1, end_pos - 1);
        if Pos(' ', strTmp) > 0 then
        begin
          exit;
        end;
        Add(TTargetChip.Create(strTmp));
      end;

      with TargetChips[ChipCount - 1] do
      begin
        FID    := 0;
        FMode  := '';
        FAreas := '';
        FExtraStr := '';
        // ID
        GetNumericParameter(line, 'id', FID);
        // Mode
        GetLiteralParameter(line, 'mode', FMode);
        // Areas
        GetLiteralParameter(line, 'area', FAreas);
        if Pos('C', CurFeature) > 0 then
        begin
          // ExtraStr as COM Setting
          FExtraStr := line;
        end;

        // TargetAreas
        TargetAreaParser(line);
        // InFile of Area
        for i := 0 to AreaCount - 1 do
        begin
          if FMode[Pos(TargetAreas[i].Name, FMode) + 1] = '1' then
          begin
            TargetAreas[i].FInFile := True;
          end
          else
          begin
            TargetAreas[i].FInFile := False;
          end;
        end;
      end;

      if (ChipCount = 1) and (TargetChips[0].Name = FName) then
      begin
        // First Target is for settings
        FMode := TargetChips[0].Mode;
        strTmp := TargetChips[0].FMode;
        TargetChips[0].FMode := '';
        repeat
          i := Pos(':', strTmp);
          if i > 0 then
          begin
            TargetChips[0].FMode := TargetChips[0].FMode + strTmp[i - 1];
            strTmp := Copy(strTmp, i + 1, Length(strTmp));
          end;
        until i = 0;
      end;
    end;
  end;
end;

end.

