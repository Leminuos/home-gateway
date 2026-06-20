#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RELEASE_DIR="${RELEASE_DIR:-${ROOT_DIR}/release}"
MACHINE="${MACHINE:-bbb-home-gateway}"
TARGETS=(core-image-home-gateway update-image)

usage() {
    cat <<EOF
Usage: ./build.sh /path/to/poky

Environment overrides:
  RELEASE_DIR  Release output directory (default: repo/release)
  MACHINE      Deploy machine name (default: bbb-home-gateway)
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ $# -ne 1 ]]; then
    usage >&2
    exit 2
fi

POKY_INPUT="$1"
if [[ ! -d "${POKY_INPUT}" ]]; then
    echo "ERROR: Poky directory does not exist: ${POKY_INPUT}" >&2
    exit 1
fi

POKY_DIR="$(cd "${POKY_INPUT}" && pwd)"
YOCTO_DIR="$(cd "${POKY_DIR}/.." && pwd)"
BUILD_DIR="${YOCTO_DIR}/build-gateway"

if [[ ! -f "${POKY_DIR}/oe-init-build-env" ]]; then
    echo "ERROR: Cannot find ${POKY_DIR}/oe-init-build-env" >&2
    echo "Pass the path to your poky directory, for example: ./build.sh ../poky" >&2
    exit 1
fi

on_error() {
    local exit_code=$?
    echo "ERROR: Build failed at line ${BASH_LINENO[0]}: ${BASH_COMMAND}" >&2
    exit "${exit_code}"
}
trap on_error ERR

echo "Repo root: ${ROOT_DIR}"
echo "Poky dir : ${POKY_DIR}"
echo "Build dir: ${BUILD_DIR}"
echo "Targets  : ${TARGETS[*]}"

cd "${YOCTO_DIR}"
set +u
source "${POKY_DIR}/oe-init-build-env" "${BUILD_DIR}" >/dev/null
set -Eeuo pipefail

for target in "${TARGETS[@]}"; do
    echo "Building ${target}..."
    bitbake "${target}"
done

DEPLOY_DIR="${BUILD_DIR}/tmp/deploy/images/${MACHINE}"
SWU_LINK="${DEPLOY_DIR}/update-image-${MACHINE}.swu"

if [[ ! -e "${SWU_LINK}" ]]; then
    mapfile -t swu_files < <(find "${DEPLOY_DIR}" -maxdepth 1 -type f -name 'update-image-*.swu' -printf '%T@ %p\n' | sort -nr | awk '{print $2}')
    if [[ "${#swu_files[@]}" -eq 0 ]]; then
        echo "ERROR: No update-image .swu artifact found in ${DEPLOY_DIR}" >&2
        exit 1
    fi
    SWU_SOURCE="${swu_files[0]}"
else
    SWU_SOURCE="$(readlink -f "${SWU_LINK}")"
fi

mkdir -p "${RELEASE_DIR}"
cp -f "${SWU_SOURCE}" "${RELEASE_DIR}/"

echo "Copied $(basename "${SWU_SOURCE}") to ${RELEASE_DIR}/"
