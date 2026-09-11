# Base image for the Linux Doom browser (WASM) port.
#
# The Emscripten toolchain is a large, stable dependency, so it belongs in the
# image (per Cursor env-setup guidance). The repo's WASM build runs later in the
# environment `install` step, where it can see the checked-out source.
FROM emscripten/emsdk:latest

# Tools Cursor and the static web server expect. emcc/node/python are already
# provided by the emscripten/emsdk image; we just ensure git + sudo + python3.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        git python3 ca-certificates sudo \
    && rm -rf /var/lib/apt/lists/*

# Match Cursor's default non-root workflow.
ARG USERNAME=ubuntu
RUN if ! id -u "$USERNAME" >/dev/null 2>&1; then \
        useradd -m -s /bin/bash "$USERNAME"; \
    fi \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# emcc is on PATH via the image's ENV; also expose it in login shells.
RUN echo 'source /emsdk/emsdk_env.sh >/dev/null 2>&1 || true' >> /etc/bash.bashrc

USER ubuntu
