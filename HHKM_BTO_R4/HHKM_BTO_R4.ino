// Original : https://github.com/ShowTakano/HHKM_Programmer/tree/master/HHKM/HHKM.ino
//
// 2022.08.10 V0.1 porting to RP2040    by B.T.O
// 2022.08.30 V0.2 Support JP Keyboaard by B.T.O
// 2022.09.01 V0.3 Switch US/JP by Serial Commnad  B.T.O
// 2022.12.27 V0.4 RP2040 bug fix by B.T.O
// 2023.05.16 V0.5 change board manager and libray version
//
//  Arduino IDE : 1.8.19
//  Board :   Seeed XIAO RP2040
//  RP2040 Board Manager and Libraries
//    Board Manager                Raspberry Pi Pico/RP2040 Ver3.2.0
//    KeyboradJP.h                 Custom Install JP_Keyboard Ver 1.0.5 by B.T.O
//    Mouse.h                      Built-In by Raspberry Pi Pico/RP2040 Ver 1.0.1
//    EEPROM.h                     Built-In by Raspberry Pi Pico/RP2040 Ver 1.0
//    Adafruit_NeoPixel.h          Custom Install Adafruit NeoPixel Ver 1.11.0 

#define KEYBOARD_JP           // V0.2 Support JP Keyboard
//#define KEYBOARD_US         // V0.2 Support JP Keyboard

#define MAGIC_NUMBER_JP 0x5a
#define MAGIC_NUMBER_US 0xa5

#include <EEPROM.h>  // Uno, Leonardo 1k=1024byte=1000文字  V0.1

#include "KeyboardJP.h"
#include "Mouse.h"

#include <Adafruit_NeoPixel.h>

#define KEY_PRINTSCREEN 0xCE

const int Num_Cmd = 10;
const int Max_Str_Len = 100;
String    Event_Command[Num_Cmd];
long      Event_IntervalSec[Num_Cmd] = {-1};
long      Event_MSecCounter[Num_Cmd] = {0}; // long整数型 -2,147,483,648から2,147,483,647までの数値
String    Event_Interval_Or_Once[Num_Cmd];
boolean   initialized = false;
int       global_index = 0;
int       msec = 0;
int       global_sec = 0;
int       _global_sec = 0;
uint8_t    magicNumber = MAGIC_NUMBER_JP;            // V0.3 

#define SERIAL_PORT_MONITOR Serial              // V0.1

// // Create the neopixel strip with the built in definitions NUM_NEOPIXEL and PIN_NEOPIXEL

#define NUM_NEOPIXEL   4                       // V0.1
#define PIN_NEOPIXEL  12                       // V0.1
#define NEO_POWER     11                       // V0.1

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_NEOPIXEL, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

void software_reset() {
  SCB->AIRCR = ((0x5FA << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk);
}

void setup() {
  EEPROM.begin(Num_Cmd * Max_Str_Len + 1);      // V0.1
  pinMode(LED_BUILTIN, OUTPUT);
  SERIAL_PORT_MONITOR.begin(9600);
  pinMode(NEO_POWER,OUTPUT);                    // V0.1 NeoPixel VCC
  digitalWrite(NEO_POWER, HIGH);                // V0.1 NeoPixel VCC
  strip.begin(); // start pixels
  strip.setBrightness(20); // not too bright!
  strip.show(); // Initialize all pixels to 'off'
  for (int index = 0; index < Num_Cmd; index++){
    initializeFromEEPROM(index * Max_Str_Len);
  }
  if (magicNumber == MAGIC_NUMBER_JP) {
    // JPキーボードで開始
    Keyboard.begin(KeyboardLayout_jp_JP);
    neo_color(30, 144, 255, 500);  // ジャパンブルー
  } else if (magicNumber == MAGIC_NUMBER_US) {
    // USキーボードで開始
    Keyboard.begin(KeyboardLayout_en_US);
    neo_color(255, 241, 0, 500);  // アメリカイエロー
  } else {
    neo_color(0, 255, 0, 500);  // 想定外の緑
    errorInit();
  }
  Mouse.begin();
  mouseMoveStarShape();
}

void errorInit(){
  // 通常動作時にはmagicNumberがJPでもUSでもない値になることはあり得ないが、
  // EEPROMに異常値が入っており他の配列から配列外アクセスでmagicNumberを上書きしたのか、
  // 緑に光りスタックする現象に遭遇
  // EEPROMをすべてゼロクリアしソフトウェアリセットして回避する
  zeroPadToEEPROM();
  delay(10);
  EEPROM.write(Num_Cmd * Max_Str_Len + 1, MAGIC_NUMBER_JP);
  EEPROM.commit();
  delay(10);
  software_reset();
}

void mouseMoveStarShape(){
  // マウスを星形に動かす
  int radius = 10;
  Mouse.move(1 * radius, 2 * radius, 0);
  delay(200);
  Mouse.move(-2 * radius, -1 * radius, 0);
  delay(200);
  Mouse.move(2 * radius, 0, 0);
  delay(200);
  Mouse.move(-2 * radius, 1 * radius, 0);
  delay(200);
  Mouse.move(1 * radius, -2 * radius, 0);
  delay(200);
}

void zeroPadToEEPROM(){
  for (int i = 0; i < Num_Cmd * Max_Str_Len; i++){
    String zero = "";
    byte len = zero.length();
    EEPROM.write(i, 0);
  }

  EEPROM.commit();
}

void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);

  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }

  EEPROM.commit();
}

String readStringFromEEPROM(int addrOffset)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen];

  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }

  return String(data);
}

void initializeFromEEPROM(int addrOffset)
{
  uint8_t workMN = EEPROM.read(Num_Cmd * Max_Str_Len + 1);

  if (workMN == 0x5a || workMN == 0xa5) {
    magicNumber = workMN;
  //if (EEPROM.read(Num_Cmd * Max_Str_Len + 1) == 0x5a) {   // check magic number V0.1
    int newStrLen = EEPROM.read(addrOffset);
    // neo_color(255, 241, 0, 50);  // EEPROM黄色10*50=500msec
    if (0 < newStrLen){
      // any data serialを受信したときに全箇所0にしている。commandを受信した箇所は文字数を入れている
      char _data[newStrLen];

      for (int i = 0; i < newStrLen; i++)
      {
        _data[i] = EEPROM.read(addrOffset + 1 + i);
      }

      String data = String(_data);
      // initializeのためにdataをparse
      String command;
      int intervalsec;
      String interval_or_once;
      parseStringToInitialize(data, command, intervalsec, interval_or_once);
      // initialize
      int index = addrOffset / Max_Str_Len;
      initialize(index, command, intervalsec, interval_or_once);
      initialized = true;
    }
  } else {
    // 工場出荷時およびプログラム書き込み後初回である。
    // neo_color(30, 144, 255, 50);  // ブルー500msec
  }
}

int myatoi(char *str) {
  // https://pknight.hatenablog.com/entry/20090629/1246254899
    int num = 0;
    while(*str != '\0'){
        num += *str - 48;
        num *= 10;
        str++;
    }
    num /= 10;
    return num;
}

void parseStringToInitialize(String data, String &command, int &intervalsec, String &interval_or_once){
  // split data to task, sec, interval
  String cmds[3];
  int _ = split(data, ',', cmds);

  // set received task, sec, interval
  String task = cmds[0];

  int task_is_type;
  task_is_type = task.indexOf("type"); // typeが含まれていれば0~。含まれていなければ-1。
  if (task_is_type != -1){
    // taskがtypeなので、taskをstrsに上書き
    // type:strでtaskに入っているので、:で二つに分割し、strsに代入
    String strs[2];
    int _ = split(task, ':', strs);
    task = strs[1];
  }
  command = task;
  // sec, intervalをparse
  // intervalsec = cmds[1].toInt(); // 0~32400 sec
  int str_len = cmds[1].length() + 1;
  char charArray[str_len];
  cmds[1].toCharArray(charArray, str_len);
  intervalsec = myatoi(charArray);
  interval_or_once = cmds[2];
  int is_once;
  is_once = interval_or_once.indexOf("once"); // onceが含まれていれば0~。含まれてなければ-1。
  if (is_once != -1){
    // onceなのでonceを返す
    String once = "once";
    interval_or_once = once;
  }
  else{
    // intervalなのでintervalを返す
    String interval = "interval";
    interval_or_once = interval;
  }
}

void blink(){
  digitalWrite(LED_BUILTIN, HIGH);
  delay(10);
  digitalWrite(LED_BUILTIN, LOW);
  //delay(10);
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  return 0;
}

void neo_rainbow(){
  for(int j = 0; j < 250; j++) { // cycles of all colors on wheel 250*4=1000=1sec
    for(int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
      strip.show();
      delay(1);
    }
  }
  neo_off();
}

void neo_color(int r, int g, int b, int msec){
  for(int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, r, g, b);
  }
  strip.show();
  delay(msec);
  neo_off();
}

void neo_off(){
  for(int k = 0; k < strip.numPixels(); k++) {
    strip.setPixelColor(k, 0x0);
  }
  strip.show();
}

void myprint(String msg, String end_str){
  SERIAL_PORT_MONITOR.print(msg);
  SERIAL_PORT_MONITOR.print(end_str);
}

int split(String data, char delimiter, String *dst){
  int index = 0;
  int arraySize = (sizeof(data))/sizeof((data[0]));
  int datalength = data.length();
  
  for(int i = 0; i < datalength; i++){
    char tmp = data.charAt(i);
    if( tmp == delimiter ){
      index++;
      if( index > (arraySize - 1)) return -1;
    }
    else dst[index] += tmp;
  }
  return (index + 1);
}

void execute(String command){
  // command実行
  int duration = 100;
  if (command == "mouse-left-click"){
    // mouse-left-click
    Mouse.click(MOUSE_LEFT);
    delay(duration);
  } else if (command == "mouse-right-click"){
    // mouse-right-click
    Mouse.click(MOUSE_RIGHT);
    delay(duration);
  } else if (command == "mouse-move"){
    // mouse-move
    Mouse.move(10, 0, 0); // 右へ10pix
    delay(duration);
    Mouse.move(-10, 0, 0); // 左へ10pix
    delay(duration);
  } else if (command == "key-up"){
    // key-up
    Keyboard.write(KEY_UP_ARROW);
    delay(duration);
  } else if (command == "key-down"){
    // key-down
    Keyboard.write(KEY_DOWN_ARROW);
    delay(duration);
  } else if (command == "key-left"){
    // key-left
    Keyboard.write(KEY_LEFT_ARROW);
    delay(duration);
  } else if (command == "key-right"){
    // key-right
    Keyboard.write(KEY_RIGHT_ARROW);
    delay(duration);
  } else if (command == "key-tab"){
    // key-tab
    Keyboard.write(KEY_TAB);
    delay(duration);
  } else if (command == "key-ecs"){
    // key-ecs
    Keyboard.write(KEY_ESC);
    delay(duration);
  } else if (command == "key-f2"){
    // key-f2
    Keyboard.write(KEY_F2);
    delay(duration);
  } else if (command == "key-f5"){
    // key-f5
    Keyboard.write(KEY_F5);
    delay(duration);
  } else if (command == "key-psc"){
    // key-psc Win+PSc
    Keyboard.press(KEY_LEFT_GUI);
    delay(duration);
    Keyboard.press(KEY_PRINTSCREEN);
    delay(duration);
    Keyboard.releaseAll();
  } else if (command == "key-gui"){
    // key-gui
    Keyboard.write(KEY_LEFT_GUI);
    delay(duration);
  } else if (command == "key-enter"){
    // key-enter
    Keyboard.write(KEY_RETURN);
    delay(duration);
  } else if (command == "key-shift-keep"){
    // key-shift-keep 押して離さない
    Keyboard.press(KEY_LEFT_SHIFT);
    delay(duration);
  } else if (command == "key-ctrl-keep"){
    // key-ctrl-keep 押して離さない
    Keyboard.press(KEY_LEFT_CTRL);
    delay(duration);
  } else if (command == "key-alt-keep"){
    // key-alt-keep 押して離さない
    Keyboard.press(KEY_LEFT_ALT);
    delay(duration);
  } else if (command == "key-del-keep"){
    // key-del-keep 押して離さない
    Keyboard.press(KEY_DELETE);
    delay(duration);
  } else if (command == "key-gui-keep"){
    // key-gui-keep 押して離さない
    Keyboard.press(KEY_LEFT_GUI);
    delay(duration);
  } else if (command == "key-release"){
    // key-release
    Keyboard.releaseAll();
    delay(duration);
  } else if (command == "terminal-win"){
    // terminal-win (KEY_LEFT_GUI cmd enter)
    Keyboard.write(KEY_LEFT_GUI);
    delay(duration);
    Keyboard.print("cmd");
    delay(duration);
    Keyboard.write(KEY_RETURN);
    delay(duration);
  } else if (command == "terminal-osx"){
    // terminal-osx (KEY_LEFT_GUI-N)
    Keyboard.press(KEY_LEFT_GUI);
    delay(duration);
    Keyboard.press('N');
    delay(duration);
    Keyboard.releaseAll();
  } else if (command == "terminal-linux"){
    // terminal-linux (ctrl-alt-t)
    Keyboard.press(KEY_LEFT_CTRL);
    delay(duration);
    Keyboard.press(KEY_LEFT_ALT);
    delay(duration);
    Keyboard.press('t');
    delay(duration);
    Keyboard.releaseAll();
  } else if (command == "log-out-win"){
    // log-out-win
    Keyboard.press(KEY_LEFT_CTRL);
    delay(duration);                // V0.4
    Keyboard.press(KEY_LEFT_ALT);
    delay(duration);                // V0.4
    Keyboard.press(KEY_DELETE);
    delay(duration);
    Keyboard.releaseAll();
    delay(2000);
    Keyboard.write(KEY_DOWN_ARROW);
    delay(duration);
    Keyboard.write(KEY_DOWN_ARROW);
    delay(duration);
    Keyboard.write(KEY_RETURN);
    delay(duration);
  } else if (command == "log-out-osx"){
    // log-out-osx
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press(KEY_LEFT_SHIFT);
    Keyboard.press('Q');
    delay(duration);
    Keyboard.releaseAll();
    Keyboard.write(KEY_RETURN);
  } else if (command == "log-out-linux"){
    // log-out-linux
    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press(KEY_DELETE);
    delay(1000);
    Keyboard.releaseAll();
    Keyboard.write(KEY_RETURN);
  } else if (command == "switch-to-us"){
    if (magicNumber != MAGIC_NUMBER_US) {
      // 現在のmagicNumverがUSじゃなかったら、USを入れ
      magicNumber = MAGIC_NUMBER_US;
      // EEPROMにUSを代入
      EEPROM.write(Num_Cmd * Max_Str_Len + 1, MAGIC_NUMBER_US);
      EEPROM.commit();
      // EEPROMに頻繁に書き込むと、書き込み中に本体を抜かれて異常値が入る可能性が増えるため
    }
  } else if (command == "switch-to-jp"){
    if (magicNumber != MAGIC_NUMBER_JP) {
      // 現在のmagicNumberがJPじゃなかったら、JPを入れ
      magicNumber = MAGIC_NUMBER_JP;
      // EEPROMに代入
      EEPROM.write(Num_Cmd * Max_Str_Len + 1, MAGIC_NUMBER_JP);
      EEPROM.commit();
      // EEPROMに頻繁に書き込むと、書き込み中に本体を抜かれて異常値が入る可能性が増えるため
    }
  } else {
    // 最後は、typeコマンドであり、入力する文字列が直接入っている場合
    int cmd_len = command.length() + 1;
    char charCommand[cmd_len];
    command.toCharArray(charCommand, cmd_len);
    Keyboard.print(charCommand);
    delay(duration);
  }
}


void initialize(int index, String command, int intervalsec, String interval_or_once){
  Event_Command[index] = command;
  Event_IntervalSec[index] = (long)intervalsec;
  Event_MSecCounter[index] = 0; // 受信してセットする度にカウンターはゼロに
  Event_Interval_Or_Once[index] = interval_or_once;
}

void event(){
  
  for (int idx=0; idx < Num_Cmd; idx++){
    // この関数は1msecごとに呼ばれ、時間カウンタをインクリメントする
    Event_MSecCounter[idx] = Event_MSecCounter[idx] + 1;
    // 時間カウンタ＝インターバル判定
    if (Event_MSecCounter[idx] == Event_IntervalSec[idx] * 1000){
      // イベント発生時間
      // do 
      execute(Event_Command[idx]);
      // blink();
      neo_rainbow();
      SERIAL_PORT_MONITOR.print(readStringFromEEPROM(idx * Max_Str_Len));
      SERIAL_PORT_MONITOR.print(";");
      // イベントを行った後には時間カウンタをゼロに戻す
      Event_MSecCounter[idx] = 0;
      // 一度しか実行しないイベントは、インターバルを-1にする
      if (Event_Interval_Or_Once[idx] == "once"){
        Event_IntervalSec[idx] = -1;
      }
    }
  }
}

void loop() {
  // inside of serial loop
  if ( SERIAL_PORT_MONITOR.available() ){
    neo_color(255, 0, 255, 500); // 受信マゼンタ500msec

    initialized = false;
    String data;
    data = SERIAL_PORT_MONITOR.readStringUntil(';');
    SERIAL_PORT_MONITOR.print("inputdata ");
    SERIAL_PORT_MONITOR.print(data);

    if (global_index == 0){
      // 受信して最初の一度目は、EEPROMを初期かする
      zeroPadToEEPROM();
      for (int i=0; i < Num_Cmd; i++){  //  すでにセットされているコマンドも初期化する
        initialize(i, "None", -1, "once");
      }
    }

    if (data == "end"){
      // endに到達
      global_index = 0;
      initialized = true;
    }

    if (initialized == false){
      // initializeのためにdataをparse
      String command;
      int intervalsec;
      String interval_or_once;
      parseStringToInitialize(data, command, intervalsec, interval_or_once);
      // initialize
      initialize(global_index, command, intervalsec, interval_or_once);
      // EEPROMに登録
      writeStringToEEPROM(global_index * Max_Str_Len, data);

      global_index = global_index + 1;
    }
  }
  // outside of serial loop
  if (initialized == true){
    event();
    delay(1);
  }
}