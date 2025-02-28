FROM nvidia/cuda:12.4.1-cudnn-devel-ubuntu22.04

RUN apt update && apt install -y curl wget git build-essential cmake pkg-config libboost-all-dev libssl-dev
RUN useradd -m -s /bin/bash runner

USER runner
WORKDIR /home/runner
COPY entrypoint.sh /home/runner/entrypoint.sh

ENTRYPOINT ["/home/runner/entrypoint.sh"]
