#!/bin/sh

set -eu

FW_SETENV="/usr/bin/fw_setenv"
FW_PRINTENV="/usr/bin/fw_printenv"

usate=$("${FW_PRINTENV}" -n ustate 2>/dev/null || echo "0")

if [ "${usate}" = "1" ]; then
    echo "OTA: Boot confirmed successfully, committing slot"
    "${FW_SETENV}" boot_count "0"
    "${FW_SETENV}" ustate     "0"
fi

exit 0
