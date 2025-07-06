#!/usr/bin/env bash

# Call the script if you changed .proto, .java files.
# Run from the project folder (containing the game.project)

# Immediately exit if any command has a non-zero exit status:
set -e

PROJECT=object_interpolation

if [ "" == "${BOB}" ]; then
    # BOB=${DYNAMO_HOME}/share/java/bob.jar
    mkdir -p build
    BOB_SHA1=${BOB_SHA1:-$(curl -s 'https://d.defold.com/stable/info.json' | jq -r .sha1)}
    BOB_LOCAL_SHA1=$((java -jar build/bob.jar --version | cut -d' ' -f6) || true)
    if [ "${BOB_LOCAL_SHA1}" != "${BOB_SHA1}" ]; then wget --progress=dot:mega -O build/bob.jar "https://d.defold.com/archive/${BOB_SHA1}/bob/bob.jar"; fi
    BOB=build/bob.jar
fi

echo "Using BOB=${BOB}"

if [ "" == "${BOB_SHA1}" ]; then
    BOB_SHA1=$(java -jar $BOB --version | awk '{print $5}')
fi

echo "Using DEFOLDSDK=${BOB_SHA1}"

if [ "" == "${BUILD_SERVER}" ]; then
    BUILD_SERVER=https://build.defold.com
fi

echo "Using BUILD_SERVER=${BUILD_SERVER}"

if [ "" == "${VARIANT}" ]; then
    VARIANT=headless
fi

echo "Using VARIANT=${VARIANT}"

TARGET_DIR=./$PROJECT/plugins
mkdir -p $TARGET_DIR

function copyfile() {
    local path=$1
    local folder=$2
    if [ -f "$path" ]; then
        if [ ! -d "$folder" ]; then
            mkdir -v -p $folder
        fi
        cp -v $path $folder
    fi
}

function copy_results() {
    local platform=$1
    local platform_ne=$2

    # Copy the .jar files
    for path in ./build/$platform_ne/$PROJECT/*.jar; do
        copyfile $path $TARGET_DIR/share
    done

    # # Copy the files to the target folder
    # for path in ./build/$platform_ne/$PROJECT/*.dylib; do
    #     copyfile $path $TARGET_DIR/lib/$platform_ne
    # done

    # for path in ./build/$platform_ne/$PROJECT/*.so; do
    #     copyfile $path $TARGET_DIR/lib/$platform_ne
    # done

    # for path in ./build/$platform_ne/$PROJECT/*.dll; do
    #     copyfile $path $TARGET_DIR/lib/$platform_ne
    # done
}


function build_plugin() {
    local platform=$1
    local platform_ne=$2

    java -jar $BOB --platform=$platform --build-artifacts=plugins --variant $VARIANT --build-server=$BUILD_SERVER --defoldsdk=$BOB_SHA1 build

    copy_results $platform $platform_ne
}


PLATFORMS=$1
if [ "" == "${PLATFORM}" ]; then
    # PLATFORMS="arm64-macos x86_64-macos x86_64-linux x86_64-win32"
    PLATFORMS="x86_64-linux"
fi

if [[ $# -gt 0 ]] ; then
    PLATFORMS="$*"
fi

echo "Building ${PLATFORMS}"

for platform in $PLATFORMS; do

    platform_ne=$platform

    if [ "$platform" == "x86_64-macos" ]; then
        platform_ne="x86_64-osx"
    fi

    if [ "$platform" == "arm64-macos" ]; then
        platform_ne="arm64-osx"
    fi

    build_plugin $platform $platform_ne
done

tree $TARGET_DIR
