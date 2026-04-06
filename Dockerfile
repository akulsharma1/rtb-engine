FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG HOST_UID=1000
ARG HOST_GID=1000
ARG USER_NAME=builder

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        build-essential \
        ca-certificates \
        clang \
        clang-format \
        cmake \
        gdb \
        git \
        iproute2 \
        ninja-build \
        pkg-config \
        protobuf-compiler \
        libprotobuf-dev \
    && rm -rf /var/lib/apt/lists/*

RUN set -eux; \
    if ! getent group "${HOST_GID}" >/dev/null; then \
        groupadd --gid "${HOST_GID}" "${USER_NAME}"; \
    fi; \
    useradd --uid "${HOST_UID}" --gid "${HOST_GID}" --create-home --shell /bin/bash "${USER_NAME}"

WORKDIR /workspace

USER "${USER_NAME}"

CMD ["/bin/bash"]
