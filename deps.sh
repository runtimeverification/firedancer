#!/usr/bin/env bash

set -euo pipefail

# Change into Firedancer root directory
cd "$(dirname "${BASH_SOURCE[0]}")"

# Fix pkg-config path and environment
source activate

# Install prefix
PREFIX="$(pwd)/opt"

help () {
cat <<EOF

  Usage: $0 [cmd] [args...]

  If cmd is ommitted, default is 'install'.

  Commands are:

    help
    - Prints this message

    check
    - Runs system requirement checks for dep build/install
    - Exits with code 0 on success

    nuke
    - Get rid of dependency checkouts
    - Get rid of all third party dependency files
    - Same as 'rm -rf $(pwd)/opt'

    fetch
    - Fetches dependencies from Git repos into $(pwd)/opt/git

    install
    - Runs 'fetch'
    - Runs 'check'
    - Builds dependencies
    - Installs all project dependencies into prefix $(pwd)/opt

EOF
  exit 0
}

nuke () {
  if [[ ! $# -eq 0 ]]; then
    echo "Unexpected arguments: $@" >&2
    exit 1
  fi
  rm -rf ./opt
  echo "[-] Nuked $(pwd)/opt"
  exit 0
}

fetch_repo () {
  # Skip if dir already exists
  if [[ -d ./opt/git/"$1" ]]; then
    echo "[~] Skipping $1 fetch as \"$(pwd)/opt/git/$1\" already exists"
    echo
    return 0
  fi

  echo "[+] Cloning $1 from $2"
  git clone "$2" "./opt/git/$1"
  echo
}

checkout_repo () {
  echo "[~] Checking out $1 $2"
  (
    cd ./opt/git/"$1"
    git -c advice.detachedHead=false checkout "$2"
  )
  echo
}

fetch () {
  mkdir -pv ./opt/git

  fetch_repo zlib https://github.com/madler/zlib
  fetch_repo zstd https://github.com/facebook/zstd
  fetch_repo elfutils git://sourceware.org/git/elfutils.git
  fetch_repo libbpf https://github.com/libbpf/libbpf
  fetch_repo openssl https://github.com/quictls/openssl

  checkout_repo zlib "v1.2.13"
  checkout_repo zstd "v1.5.4"
  checkout_repo elfutils "elfutils-0.189"
  checkout_repo libbpf "v1.1.0"
  checkout_repo openssl "OpenSSL_1_1_1t-quic1"
}

check () {
  # To-do: pkg-config, perl sanity checks
  true
}

install_zlib () {
  if pkg-config --exists zlib; then
    echo "[~] zlib already installed at $(pkg-config --path zlib), skipping installation"
    return 0
  fi

  cd ./opt/git/zlib

  echo "[+] Configuring zlib"
  ./configure \
    --prefix="$PREFIX"
  echo "[+] Configured zlib"

  echo "[+] Building zlib"
  make -j --output-sync=target libz.a
  echo "[+] Successfully built zlib"

  echo "[+] Installing zlib to $PREFIX"
  make install -j
  echo "[+] Successfully installed zlib"
}

install_zstd () {
  if pkg-config --exists libzstd; then
    echo "[~] zstd already installed at $(pkg-config --path libzstd), skipping installation"
    return 0
  fi

  cd ./opt/git/zstd/lib

  echo "[+] Installing zstd to $PREFIX"
  make -j DESTDIR="$PREFIX" PREFIX="" install-pc install-static install-includes
  echo "[+] Successfully installed zstd"
}

install_elfutils () {
  if pkg-config --exists libelf; then
    echo "[~] libelf already installed at $(pkg-config --path libelf), skipping installation"
    return 0
  fi

  cd ./opt/git/elfutils

  echo "[+] Generating elfutils configure script"
  autoreconf -i -f
  echo "[+] Generated elfutils configure script"

  echo "[+] Configuring elfutils"
  ./configure \
    --prefix="$PREFIX" \
    --enable-maintainer-mode \
    --disable-debuginfod \
    --disable-libdebuginfod \
    --without-curl \
    --without-microhttpd \
    --without-sqlite3 \
    --without-libarchive \
    --without-tests
  echo "[+] Configured elfutils"

  echo "[+] Building elfutils"
  make -j --output-sync=target
  echo "[+] Successfully built elfutils"

  echo "[+] Installing elfutils to $PREFIX"
  make install -j
  echo "[+] Successfully installed elfutils"
}

install_libbpf () {
  if pkg-config --exists libbpf; then
    echo "[~] libbpf already installed at $(pkg-config --path libbpf), skipping installation"
    return 0
  fi

  cd ./opt/git/libbpf/src

  echo "[+] Installing libbpf to $PREFIX"
  make -j install PREFIX="$PREFIX"
  echo "[+] Successfully installed libbpf"
}

install_openssl () {
  if pkg-config --exists openssl; then
    echo "[~] openssl already installed at $(pkg-config --path openssl), skipping installation"
    return 0
  fi

  cd ./opt/git/openssl

  echo "[+] Configuring OpenSSL"
  ./config \
    --prefix="$PREFIX" \
    enable-quic
  echo "[+] Configured OpenSSL"

  echo "[+] Building OpenSSL"
  make -j --output-sync=target
  echo "[+] Successfully built OpenSSL"

  echo "[+] Installing OpenSSL to $PREFIX"
  make install_sw -j
  echo "[+] Successfully installed OpenSSL"

  echo "[~] Installed all dependencies"
}

install () {
  ( install_zlib     )
  ( install_zstd     )
  ( install_elfutils )
  ( install_libbpf   )
  ( install_openssl  )
}

if [[ $# -eq 0 ]]; then
  echo "[~] This will fetch, build, and install Firedancer's dependencies into $(pwd)/opt"
  echo "[~] For help, run: $0 help"
  echo
  echo "[~] Running $0 install"
  read -p "[?] Continue? (y/N) " choice

  case "$choice" in
    y|Y)
      echo
      fetch
      check
      install
      ;;
    *)
      echo "[!] Stopping." >&2
      exit 1
  esac
fi

while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help|help)
      help
      ;;
    nuke)
      shift
      nuke
      ;;
    fetch)
      shift
      fetch
      ;;
    check)
      shift
      check
      ;;
    install)
      shift
      fetch
      check
      install
      ;;
    *)
      echo "Unknown command: $1" >&2
      exit 1
      ;;
  esac
done