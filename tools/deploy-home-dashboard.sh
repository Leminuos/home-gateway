#!/usr/bin/env bash
#
# deploy-home-dashboard.sh — build app home-dashboard bằng cross-toolchain Yocto
# rồi inject binary lên board qua SSH.
#
# Cách dùng:
#   source /path/to/environment-setup-cortexa8hf-neon-poky-linux-gnueabi
#   tools/deploy-home-dashboard.sh [BOARD_IP]
#
# Tuỳ biến qua biến môi trường (hoặc sửa mặc định bên dưới):
#   BOARD_IP    IP board (mặc định 192.168.137.100)
#   BOARD_USER  user SSH trên board (mặc định root)
#   BUILD_ONLY  =1 thì chỉ build, không inject lên board
#   CLEAN       =1 thì xoá thư mục build trước khi cấu hình lại
#
set -euo pipefail

# ============================= CONFIG ========================================
BOARD_IP="${BOARD_IP:-${1:-192.168.137.100}}"
BOARD_USER="${BOARD_USER:-root}"
TARGET_TRIPLE="arm-poky-linux-gnueabi"   # dùng để xác thực toolchain
BIN_NAME="home-gateway-app"
TARGET_BIN="/usr/bin/${BIN_NAME}"        # vị trí cài trên board
SERVICE="home-dashboard.service"
BUILD_ONLY="${BUILD_ONLY:-0}"
CLEAN="${CLEAN:-0}"
SSH_OPTS="${SSH_OPTS:--o StrictHostKeyChecking=accept-new -o ConnectTimeout=8}"
# =============================================================================

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_DIR="${REPO_ROOT}/home-gateway-app"
BUILD_DIR="${APP_DIR}/build"

log()  { printf '\033[1;34m[*]\033[0m %s\n' "$*"; }
ok()   { printf '\033[1;32m[ok]\033[0m %s\n' "$*"; }
err()  { printf '\033[1;31m[err]\033[0m %s\n' "$*" >&2; }
die()  { err "$*"; exit 1; }

# -----------------------------------------------------------------------------
# 1. Kiểm tra môi trường cross-compiler (không tự source)
# -----------------------------------------------------------------------------
ensure_cross_env() {
    # Chưa source env -> báo lỗi và hướng dẫn, không tự source.
    if [ -z "${OECORE_TARGET_SYSROOT:-}" ]; then
        err "Cross environment not active (OECORE_TARGET_SYSROOT not set)."
        err "Please source the SDK environment-setup-* script first."
        exit 1
    fi

    # Xác thực: sysroot tồn tại, compiler đúng triple và chạy được.
    [ -d "$OECORE_TARGET_SYSROOT" ]     || die "Sysroot does not exist: $OECORE_TARGET_SYSROOT"
    [ -n "${CC:-}" ]                    || die "CC empty — invalid environment"

    case "$CC" in
        *${TARGET_TRIPLE}*) : ;;
        *) die "CC does not point to ${TARGET_TRIPLE} toolchain: $CC" ;;
    esac

    command -v "${TARGET_TRIPLE}-gcc" >/dev/null 2>&1 \
        || die "${TARGET_TRIPLE}-gcc not found in PATH"

    local cc_ver
    cc_ver="$("${TARGET_TRIPLE}-gcc" -dumpversion 2>/dev/null)" \
        || die "Cross-compiler failed to run — broken environment"

    ok "Cross-compiler valid: ${TARGET_TRIPLE}-gcc ${cc_ver}"
    ok "Sysroot: $OECORE_TARGET_SYSROOT"
}

# -----------------------------------------------------------------------------
# 2. Build app bằng CMake + toolchain file của SDK
# -----------------------------------------------------------------------------
build_app() {
    local toolchain="${OECORE_NATIVE_SYSROOT}/usr/share/cmake/OEToolchainConfig.cmake"
    [ -f "$toolchain" ] || die "CMake toolchain file not found: $toolchain"

    if [ "$CLEAN" = "1" ] && [ -d "$BUILD_DIR" ]; then
        log "CLEAN=1 -> removing $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi

    if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
        log "Configuring CMake (cross)..."
        cmake -S "$APP_DIR" -B "$BUILD_DIR" \
            -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
            -DCMAKE_BUILD_TYPE=Release
    fi

    log "Building..."
    cmake --build "$BUILD_DIR" -j"$(nproc)"

    [ -f "${BUILD_DIR}/${BIN_NAME}" ] || die "Build finished but binary missing: ${BUILD_DIR}/${BIN_NAME}"

    # Cảnh báo nếu binary không phải ELF ARM (lỡ build nhầm bằng gcc host).
    if command -v file >/dev/null 2>&1; then
        file "${BUILD_DIR}/${BIN_NAME}" | grep -q "ARM" \
            || err "WARNING: binary is not ARM — check the toolchain!"
    fi
    ok "Build done: ${BUILD_DIR}/${BIN_NAME}"
}

# -----------------------------------------------------------------------------
# 3. Inject binary lên board qua SSH (stop service -> copy -> restart)
# -----------------------------------------------------------------------------
deploy_app() {
    local bin="${BUILD_DIR}/${BIN_NAME}"
    local remote="${BOARD_USER}@${BOARD_IP}"

    log "Checking connection to $remote ..."
    # shellcheck disable=SC2086
    ssh $SSH_OPTS "$remote" true \
        || die "Cannot SSH to $remote (check IP/network/SSH key)"

    log "Copying binary to /tmp ..."
    # shellcheck disable=SC2086
    scp $SSH_OPTS "$bin" "${remote}:/tmp/${BIN_NAME}.new"

    log "Stopping service ..."
    # shellcheck disable=SC2086
    ssh $SSH_OPTS "$remote" "systemctl stop '$SERVICE' 2>/dev/null || true"

    log "Replacing binary ..."
    # shellcheck disable=SC2086
    ssh $SSH_OPTS "$remote" \
        "BIN_NAME='${BIN_NAME}' TARGET_BIN='${TARGET_BIN}' sh -s" <<'REMOTE'
set -e
# Chỉ remount rw khi rootfs đang read-only (tránh log kernel thừa khi đã rw).
root_opts=$(awk '$2=="/"{print $4}' /proc/mounts)
case ",$root_opts," in
    *,ro,*) mount -o remount,rw / ;;
esac
cp "/tmp/${BIN_NAME}.new" "$TARGET_BIN"
chmod 0755 "$TARGET_BIN"
rm -f "/tmp/${BIN_NAME}.new"
sync
REMOTE

    log "Restarting service ..."
    # shellcheck disable=SC2086
    ssh $SSH_OPTS "$remote" "systemctl restart '$SERVICE'"
    # shellcheck disable=SC2086
    ssh $SSH_OPTS "$remote" "systemctl is-active '$SERVICE'" || true

    ok "Injected to $remote and restarted $SERVICE"
}

# -----------------------------------------------------------------------------
main() {
    log "Repo: $REPO_ROOT"
    ensure_cross_env
    build_app
    if [ "$BUILD_ONLY" = "1" ]; then
        ok "BUILD_ONLY=1 -> skipping inject."
        return
    fi
    deploy_app
    ok "Done."
}

main "$@"
