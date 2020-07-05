#pragma once
#include <ESP32Servo.h>
#include "Config.h"

class Rotater {
  private:
    Config* conf;
    Servo* servo;
    int pin;
    int pwm_min_width = 765;
    int pwm_max_width = 2405;
    int ms_per_degrees = 100;
    
  public:
    Rotater(Config* _conf, Servo* _servo, int servo_pin) {
      conf = _conf;
      servo = _servo;
      pin = servo_pin;
      // PWMはタイマー0を使用している
      servo->setPeriodHertz(50);
    }
    int getCurrentDegrees() { return conf->get("rotate_last_degrees").toInt(); }
    int run() {
      servo->attach(pin, pwm_min_width, pwm_max_width);
      // 現在の角度を読み込む
      int current_degrees = conf->get("rotate_last_degrees").toInt();
      int min_deg = conf->get("rotate_min_degrees").toInt();
      int max_deg = conf->get("rotate_max_degrees").toInt();
      // どちらかに回す
      if (current_degrees > 90) {
        // 角度大 -> 小さくする
        for (byte d=current_degrees; d>min_deg; d--) { servo->write(d); delay(ms_per_degrees); }
        current_degrees = min_deg;
      } else {
        // 角度小または90度 -> 大きくする
        for (byte d=current_degrees; d<max_deg; d++) { servo->write(d); delay(ms_per_degrees); }
        current_degrees = max_deg;
      }
      // サーボは解放しない（軸固定のため）
      // 転卵後の角度をセーブ
      conf->set("rotate_last_degrees", String(current_degrees));
      log_i("Egg degrees - Min %d° <= Current %d° <= Max %d°", min_deg, current_degrees, max_deg);
      // 転卵後の角度を返す
      return current_degrees;
    }
    void fix(int target_degrees = 90, bool unlock = true) {
      servo->attach(pin, pwm_min_width, pwm_max_width);
      // 現在の角度を読み込む
      int current_degrees = conf->get("rotate_last_degrees").toInt();
      // 目標角度にする
      if (current_degrees > target_degrees) {
        for (byte d=current_degrees; d>target_degrees; d--) { servo->write(d); delay(ms_per_degrees); }
      } else {
        for (byte d=current_degrees; d<target_degrees; d++) { servo->write(d); delay(ms_per_degrees); }
      }
      // 角度をセーブ
      conf->set("rotate_last_degrees", String(target_degrees));
      log_i("Egg degrees - adjusted to %d°", target_degrees);
      // サーボ解放
      if (unlock) servo->detach();
    }
};
