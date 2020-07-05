#pragma once
#include "Config.h"
#include "Display.h"
#include "Epoch.h"
#include "Heater.h"
#include "Rotater.h"
#include "THSensor.h"

/* コマンド形式は以下の3通り・全て小文字変換される
 * [取得] get KEY
 * [設定] set KEY:VALUE
 * [応答] ping,hey,hello,piyo,peep,cheep
 */
class Command {
  private:
    Config* conf;
    Epoch* epoch;
    Heater* heater;
    Rotater* rotater;
    THSensor* thsensor;
    std::function<void(bool onoff)> autoRotateFunction;
    String getter(String target) {
      // Configに対象のキーがあるか確認
      String config_data = conf->get(target);
      String result = "the value for \"" + target + "\" is not found";
      // あればその値を返す
      if (!config_data.equals("")) result = target + ":" + config_data;
      // コマンドが以下に該当すればそちらを優先する
      if (target.equals("temp")) result = "Temperature = " + String(thsensor->getTemperature());
      if (target.equals("humd")) result = "Humidity = " + String(thsensor->getHumidity());
      if (target.equals("degrees")) result = "Egg degrees = " + conf->get("rotate_last_degrees");
      if (target.equals("duty")) result = "PID duty = " + String(heater->getDuty());
      //
      return String("[GET] " + result);
    }
    String setter(String key, String val) {
      log_i("%s  %s", key, val);
      String result = "command \"" + key + "\" is not supported";
      // 自動転卵オンオフ
      if (key.equals("auto_rotate")) {
        if (val.equals("on")) {
          conf->set("rotate_onoff", "1");
          autoRotateFunction(true); // 自動転卵オン
          result = "auto_rotate (OK) set to ON";
        } else if (val.equals("off")) {
          conf->set("rotate_onoff", "0");
          autoRotateFunction(false); // 自動転卵オフ
          result = "auto_rotate (OK) set to OFF";
        } else {
          result = "auto_rotate (abort) value need to be \"on\" or \"off\" ";
        }
      }
      // 転卵角度を任意の位置にする
      if (key.equals("degrees")) {
        int d = val.toInt();
        if (conf->get("rotate_max_degrees").toInt() < d) {
          result = "degrees (abort) larger than rotate_max_degrees";
        } else if (conf->get("rotate_min_degrees").toInt() > d) {
          result = "degrees (abort) smaller than rotate_min_degrees";
        } else {
          rotater->fix(d, false);
          result = String("degrees (OK) set to ") + String(d);
        }
      }
      // 設定温度変更
      if (key.equals("temperature")) {
        float c = val.toFloat();
        if (40.0 < c) {
          result = "target temperature (abort) too hot";
        } else if (25.0 > c) {
          result = "target temperature (abort) too cold";
        } else {
          heater->setTargetTemperature(c);
          conf->set("temp_target", String(c));
          result = String("target temperature (OK) set to ") + String(c);
        }
      }
      // 孵卵開始日時（Unix時間）を設定する
      if (key.equals("start")) {
        int d = val.toInt();
        if (0 > d) {
          result = "start epoch (abort) value is out of date";
        } else if (0 == d) {
          conf->set("epoch_start", String(epoch->get()));
          result = "start epoch (OK) set to now";
        } else {
          conf->set("epoch_start", String(d));
          result = String("start epoch (OK) set to ") + String(d);
        }
      }
      // PIDパラメータ変更
      if (key.equals("kp") || key.equals("ki") || key.equals("kd")) {
        float v = val.toFloat();
        if (10.0 < v) {
          result = "PID coefficient (abort) too large";
        } else if (0.0 > v) {
          result = "PID coefficient (abort) too small";
        } else {
          conf->set(key, String(v));
          float kp = conf->get("kp").toFloat();
          float ki = conf->get("ki").toFloat();
          float kd = conf->get("kd").toFloat();
          heater->setCoefficients(kp, ki, kd);
          result = String("PID coefficients (OK) set as Kp = ") + String(kp) + String(", Ki = ") + String(ki) + String(", Kd = ") + String(kd);
        }
      }
      //
      return String("[SET] " + result);
    }
    
  public:
    Command(Config* _conf, Epoch* _epoch, Heater* _heater, Rotater* _rotater, THSensor* _thsensor) {
      conf = _conf;
      epoch = _epoch;
      heater = _heater;
      rotater = _rotater;
      thsensor = _thsensor;
    }
    void setAutoRotateFunction(std::function<void(bool onoff)> func) {
      autoRotateFunction = func;
    }
    String run(String command_message) {
      // 文字列両端の空白削除・小文字に変換
      command_message.trim();
      command_message.toLowerCase();
      // パターンごとに関数に処理を投げる
      if (command_message.indexOf("get ") == 0) {
        // getコマンド
        return getter(command_message.substring(4));
      } else if (command_message.indexOf("set ") == 0) {
        // setコマンド
        String key_val = command_message.substring(4);
        // コロンでキーと値に分ける
        int i = key_val.indexOf(":");
        if (i == -1) return "[SET] Parse error - no divider \":\" found";
        String key = key_val.substring(0, i); key.trim();
        if (key.equals("")) return "[SET] Parse error - key is empty";
        String val = key_val.substring(i+1); val.trim();
        if (val.equals("")) return "[SET] Parse error - value is empty";
        return setter(key, val);
      } else if (command_message.indexOf("ping") == 0) {
        return "Pong";
      } else if (command_message.indexOf("piyo") == 0) {
        return "Piyo";
      } else if (command_message.indexOf("reboot") == 0) {
        return "BYE";
      } else {
        return "Command not found";
      }
    }
};
