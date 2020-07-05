#pragma once
#include <DHTesp.h>

class THSensor {
  private:
    DHTesp* dht;
    float old_temp = 0.0;
    float old_humd = 0.0;
    float cur_temp = 0.0;
    float cur_humd = 0.0;
    
  public:
    THSensor(DHTesp* _dht, int pin) {
      dht = _dht;
      dht->setup(pin, DHTesp::DHT22); // DHT11とかは使いものにならないので実質DHT22(AM2302)のみ
    }
    void loop() {
      // 取得
      TempAndHumidity th = dht->getTempAndHumidity();
      cur_temp = th.temperature;
      cur_humd = th.humidity;
      if (isnan(cur_temp) || isnan(cur_humd) || dht->getStatus() != 0) {
        // エラー時は前回の値を利用する
        cur_temp = old_temp;
        cur_humd = old_humd;
        log_w("DHT22 error status: %s", String(dht->getStatusString()));
      } else {
        // 正常にとれたら値をコピーしておく
        old_temp = cur_temp;
        old_humd = cur_humd;
      }
    }
    float getTemperature() { return cur_temp; }
    float getHumidity() { return cur_humd; }
};
