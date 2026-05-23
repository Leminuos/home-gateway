# Điều khiển backlight TFT qua PWM

---

## 1. Mục tiêu

Cho phép người dùng chỉnh độ sáng màn hình TFT ngay trên màn *Settings*, thay vì backlight luôn sáng tối đa.
- Backlight được điều khiển bằng chân PWM của AM335x.
- Đổi duty cycle của PWM -> đổi độ sáng (duty cao = sáng, duty thấp = mờ). Đây là lý do dùng PWM chứ không phải GPIO bật/tắt: GPIO chỉ on/off, không dimming được.
- Đường đi: **slider (Qt)** -> `BacklightPwm` ghi sysfs -> PWM core (kernel) -> chân `ehrpwm1` -> backlight.

---

## 2. Thay đổi tóm tắt

| Tầng | Thành phần | Nội dung |
|---|---|---|
| Kernel config | `bbb_home_gateway.cfg` | Bật `CONFIG_PWM`, `CONFIG_PWM_SYSFS`, `CONFIG_PWM_TIEHRPWM`, `CONFIG_PWM_TIECAP` |
| Devicetree | `0001-bbb-home-gateway-dts.patch` | `status="okay"` cho `epwmss1` + `ehrpwm1`, pinmux ra chân backlight (`dimmy_pins`) |
| App — driver | `src/drivers/BacklightPwm.*` | Lớp điều khiển PWM qua sysfs (export/period/duty_cycle/enable) |
| App — UI | `src/ui/SettingsDashboard.*` | Slider 0–100, map qua LUT -> duty %, init lúc mở app |

---

## 3. Chi tiết từng thay đổi

### 3.1. Kernel — bật PWM ([`bbb_home_gateway.cfg`](../../meta-bsp/recipes-kernel/linux/files/bbb_home_gateway.cfg))

```
CONFIG_PWM=y
CONFIG_PWM_SYSFS=y
CONFIG_PWM_TIEHRPWM=y
CONFIG_PWM_TIECAP=y
```

- `CONFIG_PWM` + `CONFIG_PWM_TIEHRPWM`: PWM core + driver eHRPWM của AM335x (BBB).
- `CONFIG_PWM_SYSFS`: đưa PWM ra `/sys/class/pwm/pwmchipN` để userspace điều khiển từ sysfs — đây là interface mà `BacklightPwm` dùng (không cần driver backlight riêng trong kernel).

### 3.2. Devicetree — mô tả chân ([`0001-bbb-home-gateway-dts.patch`](../../meta-bsp/recipes-kernel/linux/files/0001-bbb-home-gateway-dts.patch))

```dts
&epwmss1 {
    status = "okay";
};

&ehrpwm1 {
    pinctrl-names = "default";
    pinctrl-0 = <&dimmy_pins>;
    status = "okay";
};
```

- Mặc định `ehrpwm1` bị `disabled` -> phải bật `okay` thì PWM mới hoạt động.
- `pinctrl-0 = <&dimmy_pins>` mux chân output của eHRPWM ra đúng pin nối tới mạch backlight.
- `ehrpwm1` được kernel export thành `pwmchip0` (sysfs) — khớp với đường dẫn app dùng ở mục 3.3.

### 3.3. App — driver [`BacklightPwm`](../../home-gateway-app/src/drivers/BacklightPwm.cpp)

Lớp C++ thuần điều khiển PWM hoàn toàn qua **sysfs** (`/sys/class/pwm/pwmchip0`):

- `init(chipPath, channel, periodNs)`:
  - `export` channel nếu chưa có, chờ udev tạo `pwmN` (sleep 100ms).
  - Ghi `period` rồi set `duty_cycle=0`, cuối cùng `enable=1`.
  - **Thứ tự quan trọng:** set period trước khi tăng duty để không vi phạm rule `duty_cycle ≤ period` của kernel.
- `setBrightness(percent)`: `duty = period * percent / 100`, ghi vào `duty_cycle`. Clamp `0..100`.
- `deinit()`: `duty_cycle=0` + `enable=0` (gọi ở destructor của Settings).

### 3.4. App — UI [`SettingsDashboard`](../../home-gateway-app/src/ui/SettingsDashboard.cpp)

```cpp
#define BACKLIGHT_PWM_CHIP_PATH   "/sys/class/pwm/pwmchip0"
#define BACKLIGHT_PWM_CHANNEL     0
#define BACKLIGHT_PWM_PERIOD_NS   1000000  /* 1 kHz — đủ cao để không thấy flicker */
#define BACKLIGHT_MIN_PERCENT     10       /* slider=0 vẫn sáng mờ, không tắt hẳn */
```

- Slider `0..100` không map tuyến tính sang duty mà qua bảng LUT `kBrightnessLut[101]`: mắt người cảm nhận độ sáng phi tuyến, nên duty thực đi từ ~10% -> 100% theo đường cong CIE để cảm giác "đều tay".
- **Floor 10%** (`BACKLIGHT_MIN_PERCENT`): kéo slider về 0 vẫn còn sáng mờ, tránh màn đen thui không thấy gì.
- `init` lúc dựng Settings -> set duty theo giá trị slider hiện tại; mỗi lần `valueChanged` -> `setBrightness(LUT[value])`.

---

## 4. Verify

1. Sau khi boot, kiểm tra pwmchip có mặt: `ls /sys/class/pwm/` thấy `pwmchip0`.
2. Mở màn Settings, kéo slider -> màn đổi độ sáng theo, không nhấp nháy.
3. Kiểm tra sysfs phản ánh đúng: `cat /sys/class/pwm/pwmchip0/pwm0/{period,duty_cycle,enable}`.

---

## 5. Lưu ý

- **`pwmchip0` đánh số có thể đổi:** nếu thêm/đổi PWM khác trong DTS, index `pwmchipN` có thể nhảy -> app hardcode `pwmchip0` sẽ trỏ sai. Khi đó phải sửa `BACKLIGHT_PWM_CHIP_PATH`.
- **Period 1 kHz:** đủ cao để không thấy flicker bằng mắt; nếu camera quay màn vẫn thấy sọc thì tăng tần số (giảm `period`).
- **Không lưu mức sáng:** hiện slider reset về mặc định mỗi lần khởi động (chưa persist xuống `/data` như cấu hình OTA). Có thể bổ sung sau nếu cần.