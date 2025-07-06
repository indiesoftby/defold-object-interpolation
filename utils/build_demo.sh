#!/usr/bin/env bash

# Run from the project folder (containing the game.project)

# Immediately exit if any command has a non-zero exit status:
set -e

# Set platform and architectures depending on target
PLATFORM=${PLATFORM:-wasm-web}
if [ "${PLATFORM}" == "wasm-web" ]; then
    ARCHITECTURES=${ARCHITECTURES:-wasm-web}
fi
VARIANT=${VARIANT:-release}

echo "Building for platform: ${PLATFORM}, architectures: [${ARCHITECTURES}], variant: ${VARIANT}"

# Download bob
BOB_SHA1=${BOB_SHA1:-$(curl -s 'https://d.defold.com/stable/info.json' | jq -r .sha1)}
BOB_LOCAL_SHA1=$((java -jar build/bob.jar --version | cut -d' ' -f6) || true)
if [ "${BOB_LOCAL_SHA1}" != "${BOB_SHA1}" ]; then wget --progress=dot:mega -O build/bob.jar "https://d.defold.com/archive/${BOB_SHA1}/bob/bob.jar"; fi

# Get game.project title
TITLE=$(awk -F "=" '/^title/ {gsub(/[ \r\n\t]/, "", $2); print $2}' game.project)

# Apply build settings
SETTINGS="--email a@b.com --auth 123 --texture-compression yes"
SETTINGS+=" --platform ${PLATFORM} --variant ${VARIANT}"
if [ ! -z "${ARCHITECTURES}" ]; then SETTINGS+=" --architectures ${ARCHITECTURES}"; fi
if [ ! -z "${BUILD_SERVER_URL}" ]; then SETTINGS+=" --build-server ${BUILD_SERVER_URL}"; fi
SETTINGS+=" --bundle-output build/bundle/${PLATFORM}"
SETTINGS+=" ${BUILD_SETTINGS}"

java -jar build/bob.jar ${SETTINGS} --archive clean resolve build bundle

# Run the web server
if [ "${PLATFORM}" == "wasm-web" ]; then
    if [ ! -z "${OUTPUT_DIR}" ]; then
        mv "build/bundle/${PLATFORM}/${TITLE}" "${OUTPUT_DIR}"
    else
        (cd "build/bundle/${PLATFORM}/${TITLE}" && http-server -c- -p 20000)
    fi
fi
