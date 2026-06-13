# Tạo dm-verity image từ filesystem image.
#
# Output trong IMGDEPLOYDIR:
#   <image>.<type>.verity      — data filesystem + verity hash tree append phía sau
#   <image>.<type>.verity.env  — Chứa ROOT_HASH + HASH_OFFSET
#   <link>.verity.env          — symlink trỏ tới file .env trên

CONVERSIONTYPES += "verity"
CONVERSION_DEPENDS_verity = "cryptsetup-native"
CONVERSION_CMD:verity = "verity_image_create ${IMAGE_NAME}${IMAGE_NAME_SUFFIX}.${type} ${IMAGE_LINK_NAME}.${type}.verity.env"

VERITY_BLOCK_SIZE ?= "4096"

verity_image_create() {
    input="$1"
    env_link="$2"
    output="${input}.verity"
    blksz="${VERITY_BLOCK_SIZE}"

    cp -f "${input}" "${output}"

    truncate -s "%${blksz}" "${output}"
    data_size=$(stat -L -c "%s" "${output}")

    format_output=$(veritysetup format "${output}" "${output}" \
        --data-block-size="${blksz}" \
        --hash-block-size="${blksz}" \
        --hash-offset="${data_size}")

    root_hash=$(echo "${format_output}" | awk '/^Root hash:/ { print $NF }')

    if [ -z "${root_hash}" ]; then
        bbfatal "verity_image_create: failed to get root hash from veritysetup format"
    fi

    cat > "${output}.env" <<EOF
ROOT_HASH=${root_hash}
HASH_OFFSET=${data_size}
DATA_SIZE=${data_size}
EOF

    ln -sf "${output}.env" "${env_link}"
}
