// 転卵タスク（数時間おきにタイマー割り込み）
void rotateTask(void* pvParameters) {
  while (true) {
    // 転卵周期を設定から取得
    int interval_seconds = conf.get("rotate_interval").toInt();
    // 次回転卵する時刻をセットしておく
    epoch.setNextEvent(interval_seconds);
    // 転卵実行
    rotater.run();
    // サスペンド
    vTaskSuspend(NULL);
  }
}

// 転卵停止タスク（ワンショット）
void rotateStop(void* pvParameters) {
  // 次回転卵する時刻を消す
  epoch.clearNextEvent();
  // 中立状態で停止させる
  rotater.fix(90);
  // タスク（自身）を終了させる
  vTaskDelete(NULL);
}
