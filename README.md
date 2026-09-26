## esp32-rc-car - スマホでリモコンカー操作

AndroidのChrome（Web Bluetooth）から、ESP32-C3を載せた2WD戦車式リモコンカーを操作するWebアプリ。
左右のモーターの回転差で曲がる（戦車式）。

- 丸いジョイスティックで前後・旋回を操作する（指を離すと止まる）
- 最高速度を20〜100%で制限できる（ブラウザに保存）
- 非常停止ボタン（もう一度押すまで停止し続ける）
- 電池電圧と左右モーターの出力を表示する

## ハード構成

部品の仕様・電源系・ノイズ対策などの詳細は [docs/hardware.md](docs/hardware.md) にまとめている。

```
[Android スマホ]  PWA (Web Bluetooth)
      │  BLE
      ▼
[ESP32-C3] ──PWM/方向──▶ [TB6612FNG] ──▶ 左モーター / 右モーター
      ▲ 5V                    ▲ VM
      │                       │
 [降圧DC-DC 5V] ◀─ [電源SW] ◀─ [18650 ×2 (2S 7.4V・保護回路付き)]
                              │
                              └─ 100kΩ/33kΩ 分圧 → GPIO3（電池電圧）
```

| 部位 | 部品 |
|---|---|
| マイコン | ESP32-C3 DevKitM-1 / Super Mini |
| シャーシ | 2WDシャーシキット（TTギアモーター×2＋キャスター） |
| モータードライバ | TB6612FNG モジュール |
| 電池 | 18650 ×2（2S・保護回路付き）＋ホルダー |
| 5V電源 | 降圧DC-DC（MP1584など、5V・2A以上） |
| その他 | 電源スイッチ、電解コンデンサ 470µF、セラミックコンデンサ 0.1µF ×2、抵抗 100kΩ・33kΩ |

### 配線

| TB6612FNG | 接続先 | | TB6612FNG | 接続先 |
|---|---|---|---|---|
| AIN1 | GPIO4 | | BIN1 | GPIO0 |
| AIN2 | GPIO5 | | BIN2 | GPIO1 |
| PWMA | GPIO6 | | PWMB | GPIO7 |
| AO1 / AO2 | 左モーター | | BO1 / BO2 | 右モーター |
| STBY | GPIO10 | | VCC | ESP32の3.3V |
| VM | 電池＋（電源SWの後） | | GND | 共通GND |

| その他 | 接続先 |
|---|---|
| 電池電圧 | 電池＋（電源SWの後） ─ 100kΩ ─ GPIO3 ─ 33kΩ ─ GND |
| DC-DC 出力 | ESP32の5Vピン |
| 状態LED | GPIO8（基板上のLED） |

ピンは `firmware/src/config.h` で変更できる。GPIO2/8/9（起動モードを決めるピン）とGPIO18/19（USB）は使わない。

### 電源の注意

- モーターのノイズでESP32がリセットしやすい。470µFをTB6612FNGのVM近くに、0.1µFを各モーターの端子間に直接はんだ付けする。GNDは1点で共通にする。
- TTモーターの定格は3〜6Vなので、ファームでPWMの上限を70%（8.4V×70%≒5.9V）に絞っている（`MOTOR_MAX_DUTY_PERCENT`）。
- USBで書き込むときは電池の電源スイッチをOFFにする（USBの5VとDC-DCの5Vがぶつからないようにするため）。
  シリアルモニタを見ながらモーターを回すときは、DC-DCの出力を5Vピンから外し、電池はVMにだけつなぐ。
- ESP32-C3のADCは約2.5Vまでしか正しく測れないため、分圧の下側は33kΩにする（8.4V → 約2.08V）。

## ファイル構成

Web側（ブラウザ）とファーム側（ESP32-C3）を1つのリポジトリで管理する。
ファーム側はPlatformIOプロジェクトとして `firmware/` に分離している。

```
index.html                           Web側（接続・ジョイスティック・非常停止・状態表示）
manifest.json                        PWA設定（アプリ名・アイコン・表示モード）
service-worker.js                    オフライン起動用のキャッシュ制御
icons/                               PWAアイコン（192x192 / 512x512。元データは icon.svg）
docs/hardware.md                     ハードウェア構成（部品表・配線・電源系）
esp32-rc-car.code-workspace          VS Code 用（リポジトリと firmware を同時に開く）
firmware/                            ESP32ファーム（PlatformIOプロジェクト）
  platformio.ini                     ボード・ビルド設定（NimBLE-Arduinoを使用）
  src/config.h                       ピン割り当て・UUID・速度や電圧のしきい値
  src/main.cpp                       制御ループ（100Hz）・LED表示・シリアルコマンド
  src/BleLink.*                      BLEのGATTサーバ（操作データの受信・状態の送信）
  src/DriveController.*              左右配分・加速制限・フェイルセーフ
  src/MotorDriver.*                  TB6612FNGの制御（PWM 20kHz）
  src/Battery.*                      電池電圧の測定・低電圧判定
```

## 通信仕様

独自のサービスで、バイナリの固定長データをやり取りする。
デバイス名は `ESP32-RC-Car`。UUIDは `firmware/src/config.h` と `index.html` で合わせる。

| 用途 | UUID | 属性 |
|---|---|---|
| Service | `8f1e0001-6c1b-4f5a-9d3e-2a7c5b4e1d00` | |
| Control（Web → ESP32） | `8f1e0002-6c1b-4f5a-9d3e-2a7c5b4e1d00` | Write / Write Without Response |
| Telemetry（ESP32 → Web） | `8f1e0003-6c1b-4f5a-9d3e-2a7c5b4e1d00` | Read / Notify |

### Control（3バイト、Webから50msごとに送信）

| バイト | 型 | 内容 |
|---|---|---|
| 0 | int8 | 前後 -100〜100（前進が正）。最高速度の制限はWeb側でかける |
| 1 | int8 | 旋回 -100〜100（右旋回が正） |
| 2 | uint8 | bit0: 非常停止 |

値が変わらなくても送り続ける。ESP32は300ms届かなければ止まる（フェイルセーフ）。

### Telemetry（5バイト、ESP32から250msごとに通知）

| バイト | 型 | 内容 |
|---|---|---|
| 0-1 | uint16（リトルエンディアン） | 電池電圧 mV（電池なし＝USB給電中は0） |
| 2 | uint8 | bit0: 低電圧で停止中 / bit1: 通信途絶で停止中 / bit2: 非常停止中 |
| 3 | int8 | 左モーターの出力 -100〜100 |
| 4 | int8 | 右モーターの出力 -100〜100 |

## 走行制御（ESP32側）

| 項目 | 内容 | 設定（config.h） |
|---|---|---|
| 左右配分 | 左 = 前後 + 旋回、右 = 前後 − 旋回。100を超えたら比率を保って縮める | `STEER_GAIN_PERCENT`（旋回量 60%） |
| 加速制限 | 出力を毎秒400%までしか変えない（0→100%を0.25秒） | `RAMP_PERCENT_PER_SEC` |
| フェイルセーフ | 操作データが300ms途絶えたら即停止し、STBYでモーターを切り離す | `FAILSAFE_TIMEOUT_MS` |
| 非常停止 | 加速制限をかけずに即停止（ショートブレーキ） | |
| 低電圧 | 6.4V未満が3秒続いたら停止。電源を入れ直すまで解除しない | `VBAT_LOW_MV` / `VBAT_LOW_HOLD_MS` |
| 回転方向 | 前進で逆に回るモーターはソフトで反転する | `LEFT_MOTOR_INVERT` / `RIGHT_MOTOR_INVERT` |

基板上のLEDで状態を表す：ゆっくり点滅＝接続待ち、点灯＝接続中、速い点滅＝低電圧で停止中。
GPIO8のLEDがLOWで点灯する基板を前提にしている。逆の場合は `config.h` の `LED_ON_LEVEL` / `LED_OFF_LEVEL` を入れ替える。

## 開発・実行

### ファーム側（ESP32-C3）

VS Codeで `esp32-rc-car.code-workspace` を開き、PlatformIOでビルド・書き込みする。
CLIの場合:

```bash
pio run -d firmware                  # ビルド
pio run -d firmware -t upload        # 書き込み
pio device monitor -b 115200         # シリアルモニタ
```

BLEライブラリはNimBLE-Arduinoを使っている（Flash使用量 約40%）。
書き込みに失敗する場合は、BOOTボタンを押しながらRESETを押して書き込みモードに入る。

シリアルモニタからコマンドを送ると、スマホなしでモーターを確認できる。

| コマンド | 動作 |
|---|---|
| `D <前後> <旋回>` | 1秒間だけ走る（例: `D 50 0` で前進50%、`D 0 100` でその場右旋回） |
| `S` | すぐに止める |
| `B` | 電池電圧を表示する |

### Web側

Web Bluetooth APIを使うため、HTTPSまたはlocalhostで開く必要がある。

```bash
python -m http.server 8000
# または npx serve .
# ブラウザで http://localhost:8000 を開く
```

スマホで使うときはGitHub PagesなどHTTPSで配信する。
配信ファイルを変更したら `service-worker.js` の `CACHE_VERSION` を上げる。
画面上部のタイトルを押すと最新版を確認できる（接続中は不可）。

## 動作確認の手順

1. ESP32だけで書き込み、スマホから接続して「車の状態」に `USB給電` と表示されることを確認する。
   分圧抵抗をまだ付けていない場合はGPIO3をGNDにつないでおく（浮いていると低電圧と誤判定して走らないことがある）
2. TB6612FNGとモーターをつなぎ、車輪を浮かせた状態でシリアルの `D 50 0` を送る。両輪が前進方向に回らなければ `*_MOTOR_INVERT` を変える
3. スマホのジョイスティックで操作し、指を離す・非常停止・切断でそれぞれ止まることを確認する
4. 電池駆動にして走行テストする（発進時にESP32がリセットしないか確認する）
