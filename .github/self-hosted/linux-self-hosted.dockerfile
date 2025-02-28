FROM nvidia/cuda:12.4.1-cudnn-devel-ubuntu22.04

RUN apt update && apt install -y sudo vim curl wget git build-essential cmake pkg-config libboost-all-dev libssl-dev
RUN useradd -m -s /bin/bash runner
RUN echo "runner ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

USER runner
WORKDIR /home/runner
COPY entrypoint.sh /home/runner/entrypoint.sh

RUN mkdir actions-runner
WORKDIR /home/runner/actions-runner
RUN curl -o actions-runner-linux.tar.gz -L https://github.com/actions/runner/releases/download/v2.322.0/actions-runner-linux-x64-2.322.0.tar.gz
RUN tar xzf ./actions-runner-linux.tar.gz && rm ./actions-runner-linux.tar.gz

USER root
WORKDIR /home/runner
ENTRYPOINT ["/home/runner/entrypoint.sh"]
