#pragma once
#include "FS.h"
#include "SPIFFS.h"

/* それぞれ設定値を書き込んでSPIFFSにアップロードし利用する
 * 形式は各行ごとに
 * Key:Value\n とする
 * ファイルの末尾は必ず改行すること
 * -------------------
 * 
 * rofile（読み取り専用）
 * - wifi_ssid(char*): Wi-Fi SSID
 * - wifi_passwd(char*): Wi-Fi Password
 * - ntp_interval(unsigned long): NTP時刻補正の周期（秒）
 * - rotate_interval(int): 転卵周期（秒）
 * - rotate_max_degrees(int): 最大転卵角度（度）
 * - rotate_min_degrees(int): 最小転卵角度（度）
 * - rotate_autostop_after_hours(int): 入卵後何時間で転卵を自動停止するか（18.5日後 = 444時間後）
 * - ambient_channel_id(int): AmbientチャンネルID
 * - ambient_write_key(char[16]): Ambientライトキー
 * - mqtt_client_id(char*): MQTTクライアントID
 * - mqtt_server(char*): MQTTサーバーURL
 * - mqtt_username(char*): MQTTユーザー名
 * - mqtt_passwd(char*): MQTTパスワード
 * - mqtt_incoming_topic(char*): ブローカから受信するトピック（主に設定値の変更）
 * - mqtt_outgoing_topic(char*): ブローカに送信するトピック（主に計測値の報告や設定値の確認応答）
 * 
 * rwfile（読み書き）
 * - temp_target(float): 設定温度
 * - kp,ki,kd(float): PIDの各係数
 * - epoch_start(unsigned long): 入卵時のUnixTime（開始前に0にして書き込むことでリセット）
 * - rotate_onoff(int): 転卵のオンオフ（0でオフ・1でオン）
 * - rotate_last_degrees(int): 前回終了時のサーボ位相
 * 
 */

class Config {
  private:
    const char* rofile;
    const char* rwfile;
    String ro_buffer;
    String rw_buffer;
    String read(String filename, String key) {
      String val = String("");
      File f = SPIFFS.open(filename, "r");
      while (true) {
        // 1行ずつ読み込む
        String line = f.readStringUntil('\n');
        // 何もなくなれば離脱
        if (!line.length()) break;
        // キーを探す
        int i = line.indexOf(key+":");
        if (i == 0) {
          // value先頭位置
          int s = line.indexOf(":") + 1;
          // 行末まで切り出す
          val = line.substring(s);
          break;
        }
      }
      f.close();
      return val;
    }
    
  public:
    Config() {}
    Config(const char* read_only_filename, const char* read_write_filename) {
      rofile = read_only_filename;
      rwfile = read_write_filename;
    }
    String get(String key) {
      String ro = read(rofile, key);
      String rw = read(rwfile, key);
      // ReadOnlyを優先的に返す（同じキーが書き込まれたとき対策）
      if (!ro.equals("")) return ro;
      if (!rw.equals("")) return rw;
      return "";
    }
    void getCharArray(String key, char* charArray) {
      String val = get(key);
      val.toCharArray(charArray, val.length()+1);
    }
    bool set(String key, String value) {
      // ReadWriteファイルに対象のキーがあるかどうか（無ければ終了 = 新しく行を増やすことはできない）
      if (read(rwfile, key).equals("")) {
        log_w("Not found replaceable key \"%s\" in %s", key, rwfile);
        return false;
      }
      // ReadWriteファイルを更新対象行を除いて全部読む
      String buf = String("");
      File f = SPIFFS.open(rwfile, "r");
      while (true) {
        // 1行ずつ読む
        String line = f.readStringUntil('\n');
        // 何もなくなれば離脱
        if (!line.length()) break;
        // 対象のキーが行内に存在するか
        if (line.indexOf(key) == -1) {
          // 対象のキーでなければ退避バッファに追加
          buf = buf + line + '\n';
        } else {
          // 対象のキーがあればその行は無視して新しいvalueを持った行を退避バッファに追加
          buf = buf + key + ":" + value + '\n';
        }
      }
      f.close();
      // ReadWriteファイルを削除
      bool d = SPIFFS.remove(rwfile);
      if (!d) {
        log_e("Couldn't delete: %s", rwfile);
        return false;
      }
      // ReadWriteファイルを再作成して退避していたバッファを書き込む
      File rw = SPIFFS.open(rwfile, "w");
      if (!rw) {
        log_e("Couldn't open: %s", rwfile);
        return false;
      }
      rw.print(buf);
      rw.close();
      return true;
    }
    bool setIfFalse(String key, String value) {
      // false判定（数値0か空文字列）であるときのみ値をセットする
      String target_value = read(rwfile, key);
      if (target_value.equals("") || target_value.toInt() == 0 || target_value.toFloat() == 0.0) {
        return set(key, value);
      } else {
        return false;
      }
    }
};
