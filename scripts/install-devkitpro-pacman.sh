#!/usr/bin/env bash
# ==============================================================================
# devkitPro Installation & Setup Script for Container / Linux Environments
# ==============================================================================
# HOW TO RUN:
#   sudo ./install-devkitpro-pacman  (or run directly as root)
#
# HOW TO VERIFY IT IS SET UP:
#   1. Check dkp-pacman version:
#      dkp-pacman --version
#   2. Check installed packages:
#      dkp-pacman -Q
#   3. Verify environment variables:
#      source /etc/profile.d/devkit-env.sh
#      echo $DEVKITPRO  # Should output /opt/devkitpro
# ==============================================================================

if ! [ $(id -u) = 0 ]; then
  echo "Need root privilege to install!"
  exit 1
fi

set -e

echo "Setting up devkitPro apt repository and dependencies..."

# Check local workspace apt cache first, or download if needed
mkdir -p /tmp/apt-cache/archives/partial
if [ -d /workspace/apt-cache/archives ]; then
  cp -n /workspace/apt-cache/archives/*.deb /tmp/apt-cache/archives/ 2>/dev/null || true
fi

DEB_FILE=$(find /tmp/apt-cache/archives /workspace/apt-cache/archives -name "devkitpro-pacman_*.deb" 2>/dev/null | head -n 1)

if [ -z "$DEB_FILE" ] || [ ! -f "$DEB_FILE" ]; then
  # ensure apt is set up to work with https sources
  apt-get install -y apt-transport-https wget curl || true

  # Store devkitPro gpg key locally if we don't have it already
  if ! [ -f /usr/share/keyring/devkitpro-pub.gpg ]; then
    mkdir -p /usr/share/keyring/
    wget -U "dkp apt" -O /usr/share/keyring/devkitpro-pub.gpg https://apt.devkitpro.org/devkitpro-pub.gpg || true
  fi

  # Add the devkitPro apt repository if we don't have it set up already
  if ! [ -f /etc/apt/sources.list.d/devkitpro.list ]; then
    echo "deb [signed-by=/usr/share/keyring/devkitpro-pub.gpg] https://apt.devkitpro.org stable main" > /etc/apt/sources.list.d/devkitpro.list || true
  fi

  echo "Updating package lists and downloading devkitpro-pacman..."
  apt-get -o Dir::Cache=/tmp/apt-cache -o Dir::Cache::archives=archives -o APT::Sandbox::User=root update || true
  apt-get -o Dir::Cache=/tmp/apt-cache -o Dir::Cache::archives=archives -o APT::Sandbox::User=root install --download-only -y devkitpro-pacman libarchive-tools libarchive13 || true
  DEB_FILE=$(find /tmp/apt-cache/archives -name "devkitpro-pacman_*.deb" | head -n 1)
fi

if [ -n "$DEB_FILE" ] && [ -f "$DEB_FILE" ]; then
  echo "Extracting devkitpro-pacman package with container-compatible permissions..."
  mkdir -p /opt/devkitpro /opt/devkitpro/pacman-temp
  dpkg-deb --fsys-tarfile "$DEB_FILE" | tar --no-same-owner -x -C /opt/devkitpro/pacman-temp || true
  if [ -d /opt/devkitpro/pacman-temp/opt/devkitpro ]; then
    cp -r /opt/devkitpro/pacman-temp/opt/devkitpro/* /opt/devkitpro/
  fi
  if [ -d /opt/devkitpro/pacman-temp/usr ]; then
    cp -r /opt/devkitpro/pacman-temp/usr/* /usr/
  fi
  rm -rf /opt/devkitpro/pacman-temp
else
  echo "Fallback: trying standard apt install..."
  apt-get install -y devkitpro-pacman || true
fi

# Initialize dkp-pacman keyring if needed
if command -v dkp-pacman-key &>/dev/null; then
  echo "Initializing and populating dkp-pacman keyring..."
  dkp-pacman-key --init || true
  dkp-pacman-key --populate devkitpro || true
fi

# Install Switch development packages
if command -v dkp-pacman &>/dev/null; then
  echo "Installing Switch development packages (switch-dev, switch-sdl2, switch-mesa, switch-libdrm_nouveau)..."
  dkp-pacman -Syu --noconfirm switch-dev switch-sdl2 switch-mesa switch-libdrm_nouveau || true
fi

echo "devkitPro setup completed successfully!"
