#pragma once

class Display {
  private:
    HardwareSerial* serial;
    void clear() { serial->write(0x1b); serial->write('C'); }
    void cursor_reset(int line) {
      // 指定行（1か2）のカーソルを左端に
      serial->write(0x1b); serial->write('G'); serial->write('@');
      if (line == 1) serial->write('@');
      if (line == 2) serial->write('A');
    }
    String whitespace = "                "; // 16文字
    
  public:
    Display() {}
    Display(HardwareSerial* _serial) {
      serial = _serial;
      clear();
      serial->write(0x1b); serial->write('R'); // 自動改行させない
    }
    void print(int line, String message, bool refresh = false) {
      // 毎回クリア（空白で埋める）してから表示するとちらつくので非推奨
      if (refresh) { cursor_reset(line); serial->print(whitespace); }
      cursor_reset(line); serial->print(message);
    }
    void dump(String message) {
      clear();
      serial->print(message);
    }
};
