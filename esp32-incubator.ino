#include <Ambient.h>
#include <DHTesp.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>
#include <SPIFFS.h>
#include <stdio.h>
#include <WiFi.h>
#include "Command.h"
#include "Config.h"
#include "Display.h"
#include "Epoch.h"
#include "Heater.h"
#include "Rotater.h"
#include "THSensor.h"



/**
 * 内部タイマーは4つ利用可能
 * 0 : 未割当
 * 1 : 基本タイマー（1秒毎のTick）
 * 2 : 転卵タイマー
 * 3 : 通信系タイマー（MQTT・Ambient）
 */
#define TIMER_NUM_HEARTBEAT 1
#define TIMER_NUM_ROTATER   2
#define TIMER_NUM_NETWORK   3



/**
 * 設定ファイル2種類
 * dataディレクトリ直下に入っているものを環境に合わせて編集してください
 * いずれも ESP32 Sketch Data Upload によりSPIFFSに書き込みます
 * config_filename_readonly  は Wifi情報など読み込み専用のデータ
 * config_filename_readwrite は 転卵角度や設定温度など随時で書き換えられる可能性のあるデータ
 * をそれぞれ格納しています。
 */
const char* config_filename_readonly  = "/config_ro.txt";
const char* config_filename_readwrite = "/config_rw.txt";



/**
 * 以下はピンアサイン
 * 実際のハードウェア構成に準ずるよう書き換えてください
 */
#define BUILTIN_LED       23
#define DCfan_PIN         33
#define DHTPIN            32
#define Heater_PIN        26
#define SERVO_PIN         2



//=======================================================================
//                        クラスと変数のグローバル宣言
//=======================================================================
// 設定保持クラス
Config conf;
// 時刻管理クラス
Epoch epoch;
// 温湿度センサ
DHTesp dht;
THSensor thsensor = THSensor(&dht, DHTPIN);
// 転卵
Servo servo;
Rotater rotater = Rotater(&conf, &servo, SERVO_PIN);
// 有機ELディスプレイ
Display display;
// ヒーター
Heater heater = Heater(Heater_PIN, BUILTIN_LED);
// コマンド処理
Command command = Command(&conf, &epoch, &heater, &rotater, &thsensor);
// ネットワーク関係（WiFi・Ambient・MQTT）
WiFiClient mqtt_client;
WiFiClient ambient_client;
Ambient ambient;
PubSubClient mqtt;



//=======================================================================
//                        基本タスクとタイマー（毎秒実行）
//=======================================================================
TaskHandle_t heartbeatTaskHandle = NULL;
volatile SemaphoreHandle_t heartbeatSemaphore = xSemaphoreCreateBinary(); // バイナリセマフォ
void IRAM_ATTR heartbeat() {
  // 基本タイマーの割り込み開始
  vTaskResume(heartbeatTaskHandle);
  xSemaphoreGiveFromISR(heartbeatSemaphore, NULL);
}
hw_timer_t* heartbeatTimer = NULL;
void heartbeatTask(void* pvParameters); // 実装はTest_Heartbeat.inoを参照



//=======================================================================
//                        転卵タスクとタイマー
//=======================================================================
TaskHandle_t rotateTaskHandle = NULL;
volatile SemaphoreHandle_t rotateSemaphore = xSemaphoreCreateBinary(); // バイナリセマフォ
void IRAM_ATTR rotate() {
  // 転卵タイマーの割り込み開始
  vTaskResume(rotateTaskHandle);
  xSemaphoreGiveFromISR(rotateSemaphore, NULL);
}
hw_timer_t* rotateTimer = NULL;
// タスク：一定時間ごとに転卵（繰り返し）
void rotateTask(void* pvParameters); // 実装はTest_Rotate.inoを参照
// タスク：転卵の停止（ワンショット）
void rotateStop(void* pvParameters); // 実装はTest_Rotate.inoを参照



//=======================================================================
//                        自動転卵の開始と停止
//=======================================================================
void autoRotate(bool onoff) {
  if (onoff) {
    // 転卵タイマーを設定
    rotateTimer = timerBegin(TIMER_NUM_ROTATER, 80, true);
    timerAttachInterrupt(rotateTimer, &rotate, true);
    timerAlarmWrite(rotateTimer, conf.get("rotate_interval").toInt() * 1000000UL, true);
    // 転卵タスクを再開状態にする（切り替えたらすぐに動き始めるように見える）
    vTaskResume(rotateTaskHandle);
    // 転卵タイマー開始
    timerAlarmEnable(rotateTimer);
  } else {
    // 転卵タイマー停止
    if (rotateTimer != NULL) timerEnd(rotateTimer);
    rotateTimer = NULL;
    // 転卵停止タスクを登録・即実行（実行後vTaskDeleteにより自動終了とタスク登録削除）
    xTaskCreatePinnedToCore(rotateStop, "RotaterStop", 4096, NULL, 2, NULL, PRO_CPU_NUM);
  }
}



//=======================================================================
//                        MQTTコールバック
//=======================================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // 受信したpayloadをStringに変換する
  payload[length] = '\0';
  String message = String((char*)payload);
  log_d("MQTT message received: %s", message);
  // コマンド実行して結果を得る
  String result = command.run(message);
  log_d("Command result: %s", result);
  // 結果をPublishする
  char out_topic[32], mqtt_report_message[128];
  conf.getCharArray("mqtt_outgoing_topic", out_topic);  // 送信するトピック
  epoch.getDebugCharArray(mqtt_report_message, result); // 日付つきコマンド結果
  mqtt.publish(out_topic, mqtt_report_message);
  // コマンドクラスがBYEを返す（rebootを受信）場合は再起動をかける
  if (result.equals("BYE")) { delay(1000); ESP.restart(); }
}



//=======================================================================
//                        ネットワークタスクとタイマー
//=======================================================================
TaskHandle_t networkTaskHandle = NULL;
volatile SemaphoreHandle_t networkSemaphore = xSemaphoreCreateBinary(); // バイナリセマフォ
void IRAM_ATTR network() {
  // 通信系タイマーの割り込み開始
  vTaskResume(networkTaskHandle);
  xSemaphoreGiveFromISR(networkSemaphore, NULL);
}
hw_timer_t* networkTimer = NULL;
// タスク
void networkTask(void* pvParameters); // 実装はTest_Network.inoを参照



//=======================================================================
//                        Setup
//=======================================================================
void setup() {
  // SPIFFSを開始してからconfを初期化する
  // メモリの関係？でconf内部でグローバルのSPIFFSを直接使用しているため
  SPIFFS.begin();
  conf = Config(config_filename_readonly, config_filename_readwrite);

  // USBシリアル
  Serial.begin(115200);
  // シリアル2(LCDディスプレイ用)
  Serial2.begin(9600, SERIAL_8N1, 18, 19);
  // ディスプレイ表示クラス
  display = Display(&Serial2);

  // WiFi接続
  char wifi_ssid[32], wifi_passwd[32];
  conf.getCharArray("wifi_ssid",   wifi_ssid);
  conf.getCharArray("wifi_passwd", wifi_passwd);
  log_i("Connecting to SSID \"%s\" ...", wifi_ssid);
  display.print(1, "SSID: " + String(wifi_ssid), true);
  WiFi.begin(wifi_ssid, wifi_passwd);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  log_i("Wi-Fi OK - SSID \"%s\"", wifi_ssid);
  IPAddress ipaddr = WiFi.localIP();
  display.print(2,
    String(ipaddr[0]) + String(".") +
    String(ipaddr[1]) + String(".") +
    String(ipaddr[2]) + String(".") +
    String(ipaddr[3]), true
  );
  delay(1000);

  // 時刻管理クラス（コンストラクタでNTP同期）
  display.print(1, "NTP Sync...", true);
  epoch = Epoch(conf.get("ntp_interval").toInt());
  // 同期できるまで最大30秒待つ
  while (epoch.get() < 30) {
    epoch.ntpSync();
    delay(1000);
  }
  // 日付と時刻を確認表示
  display.print(1, epoch.getDateString(), true);
  display.print(2, epoch.getTimeString(), true);
  delay(1000);

  // 開始日時を記録する
  // すでに0でないepochが記録されている場合には上書きしない
  // "ESP32 Sketch Data Upload" で CONFIG_FILE_RW の設定ファイル初期値にしてアップロードすると次回起動時には記録される
  conf.setIfFalse("epoch_start", String(epoch.get()));

  // 目標温度を設定
  heater.setTargetTemperature(conf.get("temp_target").toFloat());
  // PID係数を設定
  heater.setCoefficients(conf.get("kp").toFloat(), conf.get("ki").toFloat(), conf.get("kd").toFloat());

  // 自動転卵をオンオフする関数をコマンドクラスに登録しておく
  command.setAutoRotateFunction(autoRotate);

  // ファンは常時ONにしておく
  pinMode(DCfan_PIN, OUTPUT);
  digitalWrite(DCfan_PIN, HIGH);

  // 基本タスク登録（登録後すぐ実行）
  // タスク関数,タスク名,スタックサイズ,引数,優先度,タスクハンドラ,実行コア番号(0/1)
  // APP_CPU_NUM = 1:loop()と同じコア（WDT無効）
  xTaskCreatePinnedToCore(heartbeatTask, "Heartbeat", 8192, NULL, 3, &heartbeatTaskHandle, APP_CPU_NUM);
  delay(100);
  // xTaskCreateがpdTRUE以外を返すかハンドラがNULLであれば登録失敗
  if (heartbeatTaskHandle == NULL) { log_w("Failed to start HeartBeat Task"); }

  // 基本タイマー設定（必ず毎秒実行 = 100万μs）
  heartbeatTimer = timerBegin(TIMER_NUM_HEARTBEAT, 80, true); // (内部タイマー番号, 分周率, カウントアップ)
  timerAttachInterrupt(heartbeatTimer, &heartbeat, true);     // (hw_timer_t, 割り込み関数, エッジ割り込み)
  timerAlarmWrite(heartbeatTimer, 1000000UL, true);           // (hw_timer_t, 時間設定, 繰り返し)
  // 基本タイマー開始
  timerAlarmEnable(heartbeatTimer);

  // 通信系タスク登録（登録後すぐ実行）
  // ブロッキングする可能性があるのでWDT無効のAPP_CPUで行うのがよい
  xTaskCreatePinnedToCore(networkTask, "NetworkService", 8192, NULL, 3, &networkTaskHandle, APP_CPU_NUM);
  delay(100);
  // xTaskCreateがpdTRUE以外を返すかハンドラがNULLであれば登録失敗
  if (networkTaskHandle == NULL) { log_w("Failed to start Network Task"); }
  
  // 通信系タイマー設定（30秒毎に実行 = 3000万μs）
  networkTimer = timerBegin(TIMER_NUM_NETWORK, 80, true); // (内部タイマー番号, 分周率, カウントアップ)
  timerAttachInterrupt(networkTimer, &network, true);     // (hw_timer_t, 割り込み関数, エッジ割り込み)
  timerAlarmWrite(networkTimer, 30000000UL, true);        // (hw_timer_t, 時間設定, 繰り返し)
  // 通信系タイマー開始
  timerAlarmEnable(networkTimer);

  // 転卵タスクを登録・すぐ実行されないように一時停止
  // PRO_CPU_NUM = 0:無線などバックグラウンドと同じコア（WDT有効）
  xTaskCreatePinnedToCore(rotateTask, "RotaterRun", 4096, NULL, 2, &rotateTaskHandle, PRO_CPU_NUM);  
  vTaskSuspend(rotateTaskHandle);

  // 転卵は設定値によってタイマーを開始するか停止状態にするか決める
  // trueなら転卵タスクレジューム・falseなら転卵停止タスクがワンショット動作
  autoRotate(conf.get("rotate_onoff").equals("1"));
}



//=======================================================================
//                        Loop
//=======================================================================
void loop() {
  // loopTask()の優先度は1
  if (xSemaphoreTake(heartbeatSemaphore, 0) == pdTRUE) {
    // セマフォ処理待ち（= タスクの終了待ち）
    // タスクの末端でvTaskSuspend(NULL)されるとセマフォが返されるのでここが実行される
    // 席数が1つしかない居酒屋で「大将やってるかい」と聞くのと同じ
  }
  mqtt.loop();
  delay(50);
}
