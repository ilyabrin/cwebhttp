#!/bin/bash
# Build cwebhttp with TLS support (Unix/Linux/macOS)

set -e

echo "Building cwebhttp with TLS support..."
echo ""

# Clean previous builds
rm -rf build

# Check for mbedTLS
if ! pkg-config --exists mbedtls 2>/dev/null; then
    echo "WARNING: mbedTLS not found via pkg-config"
    echo "Install it with:"
    echo "  Ubuntu/Debian: sudo apt-get install libmbedtls-dev"
    echo "  macOS:         brew install mbedtls"
    echo ""
    echo "Attempting to build anyway..."
    echo ""
fi

# Build with TLS enabled
make ENABLE_TLS=1 \
     build/tests/test_tls \
     build/examples/https_client

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "Build successful!"
    echo "TLS support: ENABLED"
    echo "========================================"
    echo ""
    echo "Examples:"
    echo "  ./build/examples/https_client www.example.com"
    echo ""
    echo "Tests:"
    echo "  ./build/tests/test_tls"
    echo "========================================"
else
    echo ""
    echo "========================================"
    echo "Build FAILED!"
    echo ""
    echo "Make sure mbedTLS is installed:"
    echo "  Ubuntu/Debian: sudo apt-get install libmbedtls-dev"
    echo "  macOS:         brew install mbedtls"
    echo "========================================"
    exit 1
fi
