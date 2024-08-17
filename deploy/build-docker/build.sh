#!/usr/bin/env bash

cd "$(dirname "$0")" || exit
source ./VERSION

cd ../../


if [ "$1" != "--target=cuda" ]; then
  #   ______ .______    __    __
  #  /      ||   _  \  |  |  |  |
  # |  ,----'|  |_)  | |  |  |  |
  # |  |     |   ___/  |  |  |  |
  # |  `----.|  |      |  `--'  |
  #  \______|| _|       \______/
  POSTFIX=linux-cpu
  IMAGE_NAME=${IMAGE_PREFIX}:${VERSION}-${POSTFIX}

  docker buildx build --platform linux/amd64 -t ${IMAGE_NAME} -f deploy/build-docker/${POSTFIX}.dockerfile --load . || exit 1
  ./deploy/build-docker/docker-image-test.sh ${IMAGE_NAME} || exit 1
  docker buildx build --platform linux/amd64,linux/arm64 -t ${IMAGE_NAME} -f deploy/build-docker/${POSTFIX}.dockerfile --push . || exit 1
fi


if [ "$1" != "--target=cpu" ]; then
  #   ______  __    __   _______       ___         ___   ___    __    _  _
  #  /      ||  |  |  | |       \     /   \        \  \ /  /   / /   | || |
  # |  ,----'|  |  |  | |  .--.  |   /  ^  \   _____\  V  /   / /_   | || |_
  # |  |     |  |  |  | |  |  |  |  /  /_\  \ |______>   <   | '_ \  |__   _|
  # |  `----.|  `--'  | |  '--'  | /  _____  \      /  .  \  | (_) |    | |
  #  \______| \______/  |_______/ /__/     \__\    /__/ \__\  \___/     |_|
  POSTFIX=linux-cuda11
  IMAGE_NAME=${IMAGE_PREFIX}:${VERSION}-${POSTFIX}

  docker buildx build --platform linux/amd64 -t ${IMAGE_NAME} -f deploy/build-docker/${POSTFIX}.dockerfile --load . || exit 1
  ./deploy/build-docker/docker-image-test.sh ${IMAGE_NAME} 1 || exit 1
  docker buildx build --platform linux/amd64 -t ${IMAGE_NAME} -f deploy/build-docker/${POSTFIX}.dockerfile --push . || exit 1


#  POSTFIX=linux-cuda12
#  IMAGE_NAME=${IMAGE_PREFIX}:${VERSION}-${POSTFIX}

#  docker buildx build --platform linux/amd64 -t ${IMAGE_NAME} -f deploy/build-docker/${POSTFIX}.dockerfile --load . || exit 1
#  ./deploy/build-docker/docker-image-test.sh ${IMAGE_NAME} 1 || exit 1
#  docker buildx build --platform linux/amd64 -t ${IMAGE_NAME} -f deploy/build-docker/${POSTFIX}.dockerfile --push . || exit 1
fi

