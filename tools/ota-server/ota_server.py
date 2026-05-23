#!/usr/bin/env python3
"""
OTA firmware server chạy trên HOST.

Tool TEST — cấu hình fix cứng ở khối CONFIG bên dưới, không cần tham số.
Cách dùng:
  1. python3 ota_server.py
  2. Copy file .swu vào thư mục release/ (ngang hàng với tools/, ở gốc repo)

manifest.json được ghi cạnh script này (tools/ota-server/manifest.json).
"""

import hashlib
import json
import os
import re
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))   # tools/ota-server
_REPO_ROOT = os.path.dirname(os.path.dirname(_SCRIPT_DIR))  # gốc repo

# ============================= CONFIG ========================================
HOST_IP        = "192.168.137.1"   # IP của host (nhúng vào url manifest)
HTTP_PORT      = 8000
RELEASE_DIR    = os.path.join(_REPO_ROOT, "release")  # nơi đặt file .swu
MQTT_BROKER    = "127.0.0.1"
MQTT_PORT      = 1883
MQTT_TOPIC     = "ota/latest"
WATCH_INTERVAL = 5              # re-scan để tự republish khi có file mới (giây)
# =============================================================================

MANIFEST_PATH = os.path.join(_SCRIPT_DIR, "manifest.json")

# update-image-bbb-home-gateway-0.2.0.swu -> "0.2.0"
VERSION_RE = re.compile(r"-(\d[\w.\-]*)\.swu$")


def parse_version(filename):
    """Lấy version từ tên file .swu; None nếu không khớp."""
    m = VERSION_RE.search(filename)
    return m.group(1) if m else None


def version_key(ver):
    """Tách version thành tuple số để so sánh (semver-lite)."""
    key = []
    for p in ver.lstrip("vV").split("."):
        num = ""
        for ch in p:
            if ch.isdigit():
                num += ch
            else:
                break
        key.append(int(num) if num else 0)
    return tuple(key)


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


class FirmwareStore:
    """Quét RELEASE_DIR, sinh manifest cho .swu version cao nhất.
    Cache sha256 theo (mtime, size). Thread-safe."""

    def __init__(self):
        self._lock = threading.Lock()
        self._cache = {}      # filename -> (mtime, size, sha256)
        self._written = None  # nội dung manifest.json đã ghi gần nhất

    def _latest_file(self):
        best = None  # (version_key, version_str, filename)
        try:
            entries = os.listdir(RELEASE_DIR)
        except FileNotFoundError:
            return None
        for name in entries:
            if not name.endswith(".swu"):
                continue
            ver = parse_version(name)
            if ver is None:
                continue
            key = version_key(ver)
            if best is None or key > best[0]:
                best = (key, ver, name)
        return best

    def _build(self):
        best = self._latest_file()
        if best is None:
            return None
        _, version, filename = best
        path = os.path.join(RELEASE_DIR, filename)
        st = os.stat(path)

        cached = self._cache.get(filename)
        if cached and cached[0] == st.st_mtime and cached[1] == st.st_size:
            digest = cached[2]
        else:
            digest = sha256_of(path)
            self._cache[filename] = (st.st_mtime, st.st_size, digest)

        return {
            "version": version,
            "url": "http://%s:%d/release/%s" % (HOST_IP, HTTP_PORT, filename),
            "size": st.st_size,
            "sha256": digest,
        }

    def _persist(self, manifest):
        """Ghi manifest.json (cạnh script) khi nội dung đổi; xoá nếu hết file."""
        content = json.dumps(manifest, indent=2) if manifest else None
        if content == self._written:
            return
        self._written = content
        if content is None:
            try:
                os.remove(MANIFEST_PATH)
            except FileNotFoundError:
                pass
        else:
            with open(MANIFEST_PATH, "w") as f:
                f.write(content)

    def manifest(self):
        """Manifest cho version mới nhất (None nếu chưa có file).
        Đồng thời đồng bộ file manifest.json."""
        with self._lock:
            m = self._build()
            self._persist(m)
            return m


def make_handler(store):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            print("[http] %s - %s" % (self.address_string(), fmt % args))

        def _send_json(self, code, obj):
            body = json.dumps(obj, indent=2).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if self.path in ("/manifest.json", "/"):
                manifest = store.manifest()
                if manifest is None:
                    self._send_json(404, {"error": "no firmware available"})
                else:
                    self._send_json(200, manifest)
                return

            if self.path.startswith("/release/"):
                name = os.path.basename(self.path[len("/release/"):])
                path = os.path.join(RELEASE_DIR, name)
                if not name.endswith(".swu") or not os.path.isfile(path):
                    self.send_error(404, "firmware not found")
                    return
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(os.path.getsize(path)))
                self.end_headers()
                with open(path, "rb") as f:
                    while True:
                        chunk = f.read(1024 * 1024)
                        if not chunk:
                            break
                        self.wfile.write(chunk)
                return

            self.send_error(404, "not found")

    return Handler


def publish_mqtt(manifest):
    """Publish retained manifest qua mosquitto_pub. True nếu thành công."""
    cmd = [
        "mosquitto_pub",
        "-h", MQTT_BROKER,
        "-p", str(MQTT_PORT),
        "-t", MQTT_TOPIC,
        "-r",                       # retained
        "-m", json.dumps(manifest),
    ]
    try:
        subprocess.run(cmd, check=True)
        print("[mqtt] published retained %s -> version %s" % (MQTT_TOPIC, manifest["version"]))
        return True
    except FileNotFoundError:
        print("[mqtt] WARNING: mosquitto_pub not found — skipping publish")
        return False
    except subprocess.CalledProcessError as e:
        print("[mqtt] publish error: %s" % e)
        return False


def mqtt_watcher(store):
    """Luồng nền: republish khi version mới nhất đổi (sau khi copy file mới)."""
    last_version = None
    while True:
        manifest = store.manifest()
        if manifest and manifest["version"] != last_version:
            if publish_mqtt(manifest):
                last_version = manifest["version"]
        time.sleep(WATCH_INTERVAL)


def main():
    os.makedirs(RELEASE_DIR, exist_ok=True)
    store = FirmwareStore()

    manifest = store.manifest()
    if manifest:
        print("[ota] latest firmware: %s (%s)" % (manifest["version"], manifest["url"]))
    else:
        print("[ota] no .swu yet — copy a firmware file into %s" % RELEASE_DIR)

    threading.Thread(target=mqtt_watcher, args=(store,), daemon=True).start()

    httpd = ThreadingHTTPServer(("0.0.0.0", HTTP_PORT), make_handler(store))
    print("[ota] HTTP server at http://%s:%d  (manifest: /manifest.json)" % (HOST_IP, HTTP_PORT))
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[ota] stopping server")
        httpd.shutdown()


if __name__ == "__main__":
    main()
