#!/bin/sh
set -eu

FW_SETENV="/usr/bin/fw_setenv"
FW_PRINTENV="/usr/bin/fw_printenv"

case "${1:-}" in
    preinst)
        echo "Pre-install: nothing to do"
        exit 0
        ;;
    postinst)
        # Đọc slot hiện tại
        CURRENT_SLOT=$("${FW_PRINTENV}" -n active_slot 2>/dev/null || echo "A")

        # Xác định slot inactive
        if [ "${CURRENT_SLOT}" = "A" ]; then
            NEW_SLOT="B"
        else
            NEW_SLOT="A"
        fi

        echo "Current slot: ${CURRENT_SLOT}, switching to: ${NEW_SLOT}"

        # Set U-Boot env để boot vào slot mới
        "${FW_SETENV}" active_slot  "${NEW_SLOT}"
        "${FW_SETENV}" ustate       "1"
        "${FW_SETENV}" boot_count   "0"
        ;;
    *)
        echo "Unknown argument: ${1:-}"
        exit 1
        ;;
esac

exit 0