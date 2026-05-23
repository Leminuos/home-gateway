# OTA pull-model: server firmware trên host + update từ Qt app

**Ngày:** 2026-05-23
**Loại:** Thay đổi luồng OTA phía vận hành — đảo chiều từ push lên BBB sang BBB pull từ host. Phần A/B + rollback trong bootloader không đổi.

---

## 1. Mục tiêu

Trước đây admin mở web UI của SWUpdate trên chính BBB (`http://<board-ip>:8080`) rồi upload file `.swu` lên. Cách này:
- cần phải biết IP từng board và thao tác thủ công trên từng thiết bị,
- không có khái niệm "version mới nhất" tập trung,

Mục tiêu mới: một server nhỏ trên host giữ firmware, BBB chủ động phát hiện bản mới và tự cài, theo 2 chế độ:
- **Auto** — có bản mới là tự bật popup hỏi cài.
- **Manual** — user bấm *Check for updates* mới kiểm tra.

---

## 2. Thay đổi tóm tắt

| Thành phần | Trước | Sau |
|---|---|---|
| Chiều truyền | Admin push `.swu` lên web UI của BBB | BBB pull `.swu` từ server host |
| Phát hiện bản mới | Không có | Hybrid: **MQTT** retained `ota/latest` (auto) + **HTTP** `GET /manifest.json` (manual) |
| Server firmware | Web UI Mongoose trên BBB | `tools/ota-server/ota_server.py` chạy trên host |
| Cài đặt trên BBB | SWUpdate nhận file qua web UI | Qt app tải `.swu` -> **POST `localhost:8080`** cho chính SWUpdate daemon |
| Version hiện tại | Hardcode `v1.4.2` trong Qt | Đọc `/etc/sw-versions` |
| Tiến độ flash | Mô phỏng bằng `QTimer` | % thật theo `uploadProgress` (BBB stream `.swu` lên daemon, daemon ghi theo tốc độ nhận); hoàn tất qua socket `SUCCESS` hoặc fallback `fw_printenv ustate` |
| Reboot | SWUpdate tự reboot (`reboot-required = true`) | App hiện nút **Reboot now**, user bấm; SWUpdate `reboot-required = false` |
| Cấu hình mode | Không lưu (hardcode trong UI) | Lưu JSON ở **`/data/ota/config.json`** |

Lưu ý: SWUpdate daemon + webserver `:8080` **vẫn giữ nguyên** — app chỉ POST file `.swu` vào đó qua `localhost`. Toàn bộ cơ chế flash slot inactive, `switch-slot.sh`, A/B, rollback không thay đổi.

## 3. Các file thay đổi

```
home-gateway-app/
├── CMakeLists.txt                         (+ Qt5::Network, src/ota)
├── src/main.cpp                           (nối OtaManager + overlay popup)
├── src/network/MqttClient.{h,cpp}         (+ clientId)
├── src/ota/
│   ├── OtaConfig.h
│   ├── FirmwareManifest.{h,cpp}
│   ├── SwupdateProgressClient.{h,cpp}
│   ├── OtaSettings.{h,cpp}                 (Đọc/ghi config auto/manual )
│   └── OtaManager.{h,cpp}
└── src/ui/
    ├── SettingsDashboard.{h,cpp}          (signal mode/check, version thật, sync toggle)
    ├── FirmwareUpdateProgress.{h,cpp,ui}  (bỏ rebooting + mô phỏng)
    └── (FirmwareUpdatePopup không đổi)

meta-bsp/recipes-qtapp/home-dashboard/files/home-dashboard.service   (+ env OTA_*)
meta-ota/recipes-support/swupdate/files/swupdate.cfg.in              (reboot-required = false)
meta-ota/recipes-core/images/core-image-home-gateway.bbappend        (+ mountpoint /data)
tools/ota-server/{ota_server.py, README.md}                          (MỚI)
```
