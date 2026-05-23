# Chạy MQTT broker trên máy host

Từ thay đổi [changes/0003-mqtt-broker-tren-host.md](../changes/0003-mqtt-broker-tren-host.md), broker mosquitto không còn chạy trên BBB nữa mà chạy trên máy host (PC dev). BBB chỉ là MQTT client, kết nối tới broker qua mạng. Guide này hướng dẫn cách cài và chạy broker trên host và verify từ BBB.

```
   BBB (192.168.137.100)                 Host / gateway (192.168.137.1)
   +---------------------+               +-----------------------+
   | home-gateway-app    |   TCP 1883    | mosquitto broker      |
   |  (MQTT client)      | ------------> |  listener 1883 (mqtt) |
   +---------------------+               +-----------------------+
```

## 1. Cài mosquitto trên host (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients
```

## 2. Cấu hình broker listen ra LAN

Mosquitto 2.x mặc định chỉ listen `localhost`. Phải khai báo listener bind ra interface LAN để BBB kết nối được. Tạo file `/etc/mosquitto/conf.d/home-gateway.conf`:

```conf
# MQTT cho BBB client
listener 1883 0.0.0.0
protocol mqtt

# WebSockets cho dashboard web (nếu cần)
listener 9001 0.0.0.0
protocol websockets

# Lab/internal: không auth
allow_anonymous true
```

> Đây chính là nội dung `mosquitto.conf` cũ từng chạy trên BBB, chỉ thêm bind `0.0.0.0`. `allow_anonymous true` chỉ chấp nhận được trong môi trường lab/internal — production phải bật auth.

Restart broker:

```bash
sudo systemctl restart mosquitto
sudo systemctl enable mosquitto      # chạy lại sau reboot host
```

## 3. Mở firewall (nếu host bật ufw)

```bash
sudo ufw allow from 192.168.137.0/24 to any port 1883 proto tcp
sudo ufw allow from 192.168.137.0/24 to any port 9001 proto tcp
```

Cách kiểm tra host có bật ufw không:

```bash
sudo ufw status
```

## 4. Verify

Trên host — kiểm tra broker đang listen ra LAN:

```bash
ss -tlnp | grep -E '1883|9001'      # phải thấy 0.0.0.0:1883
mosquitto_sub -h 192.168.137.1 -t 'sensor/#' -v
```

Bật BBB lên. App publish `sensor/temp`, `sensor/humi`, `sensor/lux` mỗi 3 giây, nên `mosquitto_sub` ở trên sẽ in ra dữ liệu.

Trên BBB cần package `mosquitto-clients`, chỉ có trong development build:

```bash
mosquitto_pub -h 192.168.137.1 -t 'test/bbb' -m 'hello from bbb'
journalctl -u home-dashboard -f        # thấy "Connected to MQTT broker"
```

## 5. Đổi địa chỉ broker

IP broker không hardcode trong app — đổi qua env trong `home-dashboard.service`:

```ini
Environment=MQTT_BROKER_HOST=192.168.137.1
Environment=MQTT_BROKER_PORT=1883
```

Sửa file này rồi rebuild image hoặc sửa trực tiếp `/etc/systemd/system/home-dashboard.service` trên BBB rồi `systemctl daemon-reload && systemctl restart home-dashboard` để test nhanh. Không cần rebuild app khi chỉ đổi địa chỉ.
