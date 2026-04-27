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
        clangd \
        clang-format \
        cmake \
        gdb \
        git \
        iproute2 \
        ninja-build \
        pkg-config \
        python3 \
        python3-pip \
        python3-protobuf \
        python3-venv \
        protobuf-compiler \
        libprotobuf-dev \
    && rm -rf /var/lib/apt/lists/*

RUN set -eux; \
    existing_group="$(getent group "${HOST_GID}" | cut -d: -f1 || true)"; \
    existing_user="$(getent passwd "${HOST_UID}" | cut -d: -f1 || true)"; \
    if [ -n "${existing_group}" ] && [ "${existing_group}" != "${USER_NAME}" ]; then \
        groupmod --new-name "${USER_NAME}" "${existing_group}"; \
    elif [ -z "${existing_group}" ]; then \
        groupadd --gid "${HOST_GID}" "${USER_NAME}"; \
    fi; \
    if id -u "${USER_NAME}" >/dev/null 2>&1; then \
        usermod --uid "${HOST_UID}" --gid "${HOST_GID}" --shell /bin/bash "${USER_NAME}"; \
    elif [ -n "${existing_user}" ]; then \
        usermod --login "${USER_NAME}" --home "/home/${USER_NAME}" --move-home --gid "${HOST_GID}" --shell /bin/bash "${existing_user}"; \
    else \
        useradd --uid "${HOST_UID}" --gid "${HOST_GID}" --create-home --shell /bin/bash "${USER_NAME}"; \
    fi

WORKDIR /workspace

USER "${USER_NAME}"

CMD ["/bin/bash"]
