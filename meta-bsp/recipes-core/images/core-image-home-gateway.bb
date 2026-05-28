require recipes-core/images/core-image-base.bb

# =============================================
# APPLICATION PACKAGES
# =============================================
IMAGE_INSTALL:append = " \
    openssh openssh-sshd \
    libmosquitto1 \
    libgpiod-tools libgpiod \
    ttf-dejavu-sans fontconfig \
    qtbase \
    home-dashboard bbb-static-ip \
"

# ============================================================
# DEV PACKAGES - debug/development tools
# Bật/tắt bằng cách set DEVELOPMENT_BUILD trong local.conf:
#   DEVELOPMENT_BUILD = "1"  → phase development
#   DEVELOPMENT_BUILD = "0"  → phase production
# ============================================================

DEV_PACKAGES = " \
    i2c-tools \
    evtest \
    systemd-analyze \
    tslib tslib-calibrate tslib-tests \
    mosquitto-clients \
    inotify-tools \
    strace \
    lsof \
"

DEVELOPMENT_BUILD ?= "0"
IMAGE_INSTALL:append = "${@'${DEV_PACKAGES}' if d.getVar('DEVELOPMENT_BUILD') == '1' else ''}"

# =============================================
# KERNEL MODULES 
# =============================================
IMAGE_INSTALL:append = " \
    kernel-module-ili9341 \
    kernel-module-drm-mipi-dbi \
"

# ============================================================
# IMAGE SIZE OPTIMIZATION
# ============================================================

# Chỉ giữ tối thiểu locale
IMAGE_LINGUAS = ""