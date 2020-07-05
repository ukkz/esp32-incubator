// 基本タスク
// 毎秒実行する（高精度なloopみたいなもの）
void heartbeatTask(void *pvParameters) {
  while (true) {
    // ループ内で利用する基準時刻を更新（他メソッドでは更新しないのでループ内では同じ時刻が取得できる）
    epoch.loop();
    // 温湿度を取得
    thsensor.loop();
    float temp = thsensor.getTemperature();
    float humd = thsensor.getHumidity();
    // ヒーター制御
    heater.loop(&epoch, temp);
    // 現在の転卵角度（位相）
    int egg_rotate_degrees = rotater.getCurrentDegrees();
    // 加温開始からの時間
    unsigned long progress = epoch.get() - conf.get("epoch_start").toInt();
    // 自動転卵のオンオフ
    bool rotate_onoff = conf.get("rotate_onoff").equals("1");
    // 転卵を自動で停止するまでの時間
    long arstop_until = (3600UL * conf.get("rotate_autostop_after_hours").toInt()) - progress;
    // 上記が0になったtickでのみ自動転卵をオフにする
    if (arstop_until == 0) {
      epoch.clearNextEvent(); // 転卵カウントダウンを0に
      conf.set("rotate_onoff", String(0)); // 設定を書き換える
      autoRotate(false); // 転卵タイマーを解除してカゴを中立位置に
    }
    // 負数になっていればすべて0にしておく
    if (arstop_until < 0) arstop_until = 0;
    
    // OLED表示1行目（温度・湿度・次回の転卵までの分数）
    char oled_line_buffer[20];
    sprintf(oled_line_buffer, "%.1fc %.1f %02lu:%02lu", temp, humd, epoch.getUntilNextEvent()/60, epoch.getUntilNextEvent()%60);
    display.print(1, String(oled_line_buffer));

    // OLED表示2行目（3秒ずつ切替）
    switch (epoch.getStep(18)) {        
      case 0: case 1: case 2:
        // 目標温度との差
        sprintf(oled_line_buffer, "Temp diff: %+.1fc", temp - conf.get("temp_target").toFloat()); break;
      case 3: case 4: case 5:
        // PID操作量
        sprintf(oled_line_buffer, "PID duty: %+.3f", heater.getDuty()); break;
      case 6: case 7: case 8:
        // 転卵角度
        sprintf(oled_line_buffer, "Egg degrees: %d  ", egg_rotate_degrees); break;
      case 9: case 10: case 11:
        // 自動転卵のオンオフ状態
        sprintf(oled_line_buffer, "Auto rotate: %s  ", (rotate_onoff ? "ON" : "OFF")); break;
      case 12: case 13: case 14:
        // 経過日数
        sprintf(oled_line_buffer, "[Day%lu] %02lu:%02lu:%02lu ", (progress/86400UL)+1, (progress%86400UL)/3600, (progress%3600)/60, (progress%3600)%60); break;
      case 15: case 16: case 17:
        // 転卵停止までの時間
        sprintf(oled_line_buffer, "Rstop: %lu:%02lu:%02lu  ", arstop_until/3600, (arstop_until%3600)/60, (arstop_until%3600)%60); break;
      default:
        sprintf(oled_line_buffer, "----------------");
    }
    display.print(2, String(oled_line_buffer));
    
    // サスペンド（RTOSに処理を返す）
    delay(1);
    vTaskSuspend(NULL);
  }
}
