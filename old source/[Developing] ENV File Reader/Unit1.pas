unit Unit1;

interface

uses
  Windows, Messages, SysUtils, Variants, Classes, Graphics, Controls, Forms,
  Dialogs, StdCtrls;

type
  TForm1 = class(TForm)
    Button1: TButton;
    Memo1: TMemo;
    procedure Button1Click(Sender: TObject);
  private
    { Private declarations }
  public
    { Public declarations }
  end;

var
  Form1: TForm1;

implementation

{$R *.dfm}

procedure TForm1.Button1Click(Sender: TObject);
var
  f:File;
  StrLength:integer;
  Index:integer;
  Str:byte;
  Text:String;
  x,i,y:Integer;
begin
  AssignFile(F,'.\n.env');
  Reset(F,1);
  Seek(F,$28);

  i:=0;
  while i<2 do
  begin
    BlockRead(F,Index,4);
    If Index=1 then
    begin
      i:=i+1;
      If i=2 then
      begin
        Break;
      end;
    end;
    BlockRead(F,StrLength,4);

    Text:='';
    For x:=1 to StrLength do
    begin
      BlockRead(F,Str,SizeOf(1));
      Text:=Text+Char(Str);
    end;
    Memo1.Lines.Add('Grass type '+IntToStr(Index)+' = '+Text+'.gb');
  end;

  //Getting the ofset of the first texture
  For x:=0 to FileSize(F)-1 do
  begin
    Seek(F,X);
    Text:='';
    For y:=1 to 5 do
    begin
      BlockRead(F,Str,1);
      Text:=Text+Char(Str);
    end;
    If Text='b_001' then
    begin
      break;
    end;
  end;

  //Moving 4 bytes back;
  Seek(F,x-4);

  //Reading to end of file
  y:=0;
  while FilePos(F)<FileSize(F)-1 do
  begin
    try
      y:=y+1;
      BlockRead(F,StrLength,4);

      Text:='';
      For x:=1 to StrLength do
      begin
        BlockRead(F,Str,1);
        Text:=Text+Char(Str);
      end;
      Memo1.Lines.Add('Texure index '+IntToStr(y)+' = '+Text+'.gtx');
      BlockRead(F,StrLength,4);
      BlockRead(F,StrLength,4);
    except
      //When an error occures, its because reading beyond EOF... so break the loop
      Break;
    end;
  end;


  closeFile(F);
end;



end.
