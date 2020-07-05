#pragma once

class Heater {
  private:
    int heater_pin;
    int indicator_pin;
    float target_temp;
    int i_steps = 600; // 積分区間長（秒）
    int d_steps = 120; // 微分区間長（秒）
    float i_temp_array[600]; // 最大10分
    float d_temp_array[600]; // 最大10分
    float kp = 1.0;
    float ki = 1.0;
    float kd = 1.0;
    int pwm_steps = 10; // この秒数のうち何秒間ONにするか？でヒーター強度を調整している
    float duty = 0.0; // 操作量
    
  public:
    Heater(int pin, int indicator_led_pin = 13) {
      // ピン設定
      heater_pin = pin;
      indicator_pin = indicator_led_pin;
      pinMode(heater_pin,    OUTPUT);
      pinMode(indicator_pin, OUTPUT);
      digitalWrite(heater_pin,    LOW);
      digitalWrite(indicator_pin, LOW);
      // 配列の初期化
      // ホットスタート時にオーバーシュートしすぎないよう全て38.5度とする）
      // コールドスタート時は加熱開始まで時間がかかることがある
      for (int t=0; t<i_steps; t++) i_temp_array[t] = 38.5;
      for (int t=0; t<d_steps; t++) d_temp_array[t] = 38.5;
    }
    void setTargetTemperature(float temp) {
      target_temp = temp;
      log_i("Set target temperature: %f°C", target_temp);
    }
    void setCoefficients(float Kp, float Ki, float Kd) {
      kp = Kp; ki = Ki; kd = Kd;
      log_i("Set PID coefficients: Kp = %f, Ki = %f, Kd = %f", kp, ki, kd);
    }
    void loop(Epoch* epoch, float current_temperature) {
      // I - 積分パラメータ
      long i_step = epoch->getStep(i_steps);
      i_temp_array[i_step] = current_temperature; // 配列に格納
      // D - 微分パラメータ
      long d_step = epoch->getStep(d_steps);
      d_temp_array[d_step] = current_temperature; // 配列に格納
      // 微分区間のうち現在から一番遠い値との差分をとる（現在値のほうが高いと正数）
      float dTempDiff = 0.0;
      if (d_step == (d_steps-1)) {
        dTempDiff = d_temp_array[d_steps-1] - d_temp_array[0]; // 現在のstepが配列末尾の場合のみ
      } else {
        dTempDiff = d_temp_array[d_step] - d_temp_array[d_step+1]; // 通常時
      }

      // P: 目標値との差（目標値より計測値が低いと正数）
      float P = target_temp - current_temperature;
      // I: i_steps区間内の目標値との差の総合計（目標値より計測値が低いと正数）
      float I = 0.0; for (long it=0; it<i_steps; it++) I += (target_temp - i_temp_array[it]);
      // D: d_steps区間内の平均変化率（目標値に関係なく計測値が上昇傾向であれば正数）
      float D = dTempDiff / d_steps;

      // IとDの次元を揃える（必要性検討）
      I = I / i_steps;
      D = D * d_steps;

      // 操作量を計算
      duty = (kp * P) + (ki * I) - (kd * D);
      // 正規化する（というか絶対値1を超えないようにするだけ）
      float duty_normalized = duty;
      if (duty < -1.0) duty_normalized = -1.0;
      if (duty > 1.0)  duty_normalized = 1.0;
      // レベル値（0~pwm_steps）に変換する（標準は10まで）
      float level_f = duty_normalized * (float)pwm_steps;
      // 小数部を四捨五入
      int level = (int)level_f;
      float n_diff = level_f - (float)level; 
      if (n_diff >= 0.5)  level += 1;
      if (n_diff <= -0.5) level -= 1;
      // レベル値が正のときは加熱・負のときは冷却
      if (level >= 0) {
        // ヒーター制御
        if (epoch->getStep(pwm_steps) < level) {
          // SSR通電
          digitalWrite(heater_pin,    HIGH);
          digitalWrite(indicator_pin, HIGH);
        } else {
          // SSR切断
          digitalWrite(heater_pin,    LOW);
          digitalWrite(indicator_pin, LOW);
        }
      } else {
        // 排気ファン制御が必要なときはこっちに書く
        // 現時点では自然冷却のため未実装
        digitalWrite(heater_pin,    LOW);
        digitalWrite(indicator_pin, LOW);
      }
    }
    float getDuty() { return duty; }
};
