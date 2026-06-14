#!/usr/bin/env bash
#
# screenshot-board.sh — chụp ảnh màn hình ILI9341 trên board.
#
# Luồng: SSH vào board -> đọc geometry framebuffer + raw data ->
# kéo về host -> giải mã raw thành PNG, lưu tại assets/tmp/yyyymmdd_hhmmss.png.
#
# Cách dùng:
#   tools/screenshot-board.sh [BOARD_IP]
#
# Tuỳ biến qua biến môi trường (hoặc sửa mặc định bên dưới):
#   BOARD_IP    IP board (mặc định 192.168.137.100)
#   BOARD_USER  user SSH trên board (mặc định root)
#   FB_DEV      framebuffer device trên board (mặc định /dev/fb0)
#
set -euo pipefail

# ============================= CONFIG ========================================
BOARD_IP="${BOARD_IP:-${1:-192.168.137.100}}"
BOARD_USER="${BOARD_USER:-root}"
FB_DEV="${FB_DEV:-/dev/fb0}"
SSH_OPTS="${SSH_OPTS:--o StrictHostKeyChecking=accept-new -o ConnectTimeout=8}"
# =============================================================================

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${REPO_ROOT}/assets/tmp"

FB_NAME="$(basename "$FB_DEV")"
FB_SYS="/sys/class/graphics/${FB_NAME}"

log()  { printf '\033[1;34m[*]\033[0m %s\n' "$*"; }
ok()   { printf '\033[1;32m[ok]\033[0m %s\n' "$*"; }
err()  { printf '\033[1;31m[err]\033[0m %s\n' "$*" >&2; }
die()  { err "$*"; exit 1; }

SSH="ssh ${SSH_OPTS} ${BOARD_USER}@${BOARD_IP}"

# -----------------------------------------------------------------------------
# 1. Kiểm tra kết nối + công cụ
# -----------------------------------------------------------------------------
command -v python3 >/dev/null 2>&1 || die "python3 not found on host"

log "Ping SSH ${BOARD_USER}@${BOARD_IP} ..."
$SSH true || die "Cannot SSH to board ${BOARD_IP}"

# -----------------------------------------------------------------------------
# 2. Đọc geometry framebuffer (width, height, bpp, stride)
# -----------------------------------------------------------------------------
log "Reading framebuffer geometry from ${FB_SYS} ..."
GEO="$($SSH "
    vs=\$(cat ${FB_SYS}/virtual_size 2>/dev/null)
    bpp=\$(cat ${FB_SYS}/bits_per_pixel 2>/dev/null)
    stride=\$(cat ${FB_SYS}/stride 2>/dev/null)
    echo \"\${vs%,*} \${vs#*,} \${bpp} \${stride:-0}\"
")"

read -r FB_W FB_H FB_BPP FB_STRIDE <<<"$GEO"
[ -n "${FB_W:-}" ] && [ -n "${FB_H:-}" ] && [ -n "${FB_BPP:-}" ] \
    || die "Failed to read framebuffer geometry (got: '$GEO')"

# Stride không có trong sysfs -> tính theo width*bpp/8.
if [ "${FB_STRIDE}" = "0" ] || [ -z "${FB_STRIDE}" ]; then
    FB_STRIDE=$(( FB_W * FB_BPP / 8 ))
fi

ok "Geometry: ${FB_W}x${FB_H}, ${FB_BPP}bpp, stride=${FB_STRIDE}"

# -----------------------------------------------------------------------------
# 3. Kéo raw framebuffer về host
# -----------------------------------------------------------------------------
RAW_BYTES=$(( FB_STRIDE * FB_H ))
RAW_TMP="$(mktemp -t fbraw.XXXXXX)"
trap 'rm -f "$RAW_TMP"' EXIT

log "Grabbing ${RAW_BYTES} bytes from board ${FB_DEV} ..."
$SSH "dd if=${FB_DEV} bs=${RAW_BYTES} count=1 2>/dev/null" > "$RAW_TMP"
[ -s "$RAW_TMP" ] || die "Empty framebuffer data"

# -----------------------------------------------------------------------------
# 4. Giải mã raw -> PNG trên host
# -----------------------------------------------------------------------------
mkdir -p "$OUT_DIR"
OUT_FILE="${OUT_DIR}/$(date +%Y%m%d_%H%M%S).png"

log "Decoding to PNG ..."
FB_W="$FB_W" FB_H="$FB_H" FB_BPP="$FB_BPP" FB_STRIDE="$FB_STRIDE" \
RAW_TMP="$RAW_TMP" OUT_FILE="$OUT_FILE" python3 - <<'PY'
import os, struct, zlib

w      = int(os.environ["FB_W"])
h      = int(os.environ["FB_H"])
bpp    = int(os.environ["FB_BPP"])
stride = int(os.environ["FB_STRIDE"])
raw    = open(os.environ["RAW_TMP"], "rb").read()
out    = os.environ["OUT_FILE"]

def rgb565(buf, w, h, stride):
    rows = bytearray()
    for y in range(h):
        rows.append(0)  # PNG filter type 0
        base = y * stride
        for x in range(w):
            o = base + x * 2
            px = buf[o] | (buf[o + 1] << 8)        # little-endian
            r = (px >> 11) & 0x1F
            g = (px >> 5)  & 0x3F
            b =  px        & 0x1F
            rows += bytes(((r * 255 + 15) // 31,
                           (g * 255 + 31) // 63,
                           (b * 255 + 15) // 31))
    return rows

def rgb8888(buf, w, h, stride):
    # linuxfb 32bpp thường là BGRX/BGRA little-endian: byte0=B,1=G,2=R.
    rows = bytearray()
    for y in range(h):
        rows.append(0)
        base = y * stride
        for x in range(w):
            o = base + x * 4
            b, g, r = buf[o], buf[o + 1], buf[o + 2]
            rows += bytes((r, g, b))
    return rows

if bpp == 16:
    rows = rgb565(raw, w, h, stride)
elif bpp in (24, 32):
    rows = rgb8888(raw, w, h, stride) if bpp == 32 else None
    if rows is None:
        # 24bpp: byte0=B,1=G,2=R, 3 byte/pixel
        rows = bytearray()
        for y in range(h):
            rows.append(0)
            base = y * stride
            for x in range(w):
                o = base + x * 3
                rows += bytes((raw[o + 2], raw[o + 1], raw[o]))
else:
    raise SystemExit(f"Unsupported bpp: {bpp}")

def png_chunk(tag, data):
    c = tag + data
    return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)

with open(out, "wb") as f:
    f.write(b"\x89PNG\r\n\x1a\n")
    f.write(png_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))  # 8-bit RGB
    f.write(png_chunk(b"IDAT", zlib.compress(bytes(rows), 9)))
    f.write(png_chunk(b"IEND", b""))
PY

ok "Saved screenshot: ${OUT_FILE}"
