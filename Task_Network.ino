// ネットワークサービスタスク（Ambient・MQTT）
void networkTask(void* pvParameters) {
  // Ambient：初期化
  char amb_wk[20]; conf.getCharArray("ambient_write_key", amb_wk);
  bool amb = ambient.begin((unsigned int)conf.get("ambient_channel_id").toInt(), amb_wk, &ambient_client);
  if (amb) {
    log_i("Ambient intialize OK");
  } else {
    log_w("Ambient intialize failure");
  }
  
  // MQTT：接続設定情報を読み出す
  String status_message = String("Ready to connect");
  char client_id[32], server[64], user[32], passwd[32], in_topic[32], out_topic[32];
  conf.getCharArray("mqtt_client_id", client_id);
  conf.getCharArray("mqtt_server", server);
  conf.getCharArray("mqtt_username", user);
  conf.getCharArray("mqtt_passwd", passwd);
  conf.getCharArray("mqtt_incoming_topic", in_topic);
  conf.getCharArray("mqtt_outgoing_topic", out_topic);
  mqtt.setClient(mqtt_client);
  // MQTT：サーバー（ブローカ）を設定
  mqtt.setServer(server, 1883);
  // MQTT：何か受信したらcallback実行登録
  mqtt.setCallback(mqttCallback);
  
  // 以下ループ
  char mqtt_report_message[64];
  while (true) {
    // Ambientアップロード
    char tempbuf[10];
    dtostrf(thsensor.getTemperature(), 3, 1, tempbuf);
    ambient.set(1, tempbuf);
    ambient.set(2, thsensor.getHumidity());
    ambient.set(3, heater.getDuty());
    ambient.set(4, rotater.getCurrentDegrees());
    if (ambient.send()) {
      log_i("Ambient - Data sent");
    } else {
      log_w("Ambient - Failed to send data");
    }
        
    // WiFi接続されているがMQTT接続されていない場合：MQTT接続試行
    if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) {
      if (mqtt.connect(client_id, user, passwd)) {
        // 接続成功
        epoch.getDebugCharArray(mqtt_report_message, "CONNECT");
        mqtt.publish(out_topic, mqtt_report_message);
        mqtt.subscribe(in_topic);
        log_i("MQTT connected to \"%s\"", server);
      } else {
        // 接続失敗
        log_w("Failed to connect MQTT server \"%s\" with user %s, client_id %s", server, user, client_id);
      }
    }
    
    // MQTTステータスログは毎回表示する
    int mqst = mqtt.state();
    if (mqst == 0) {
      log_i("MQTT - keeping alive");
    } else {
      log_w("MQTT - status warning - code %d", mqst);
    }
    
    // サスペンド（RTOSに処理を返す）
    delay(1);
    vTaskSuspend(NULL);
  }
}
