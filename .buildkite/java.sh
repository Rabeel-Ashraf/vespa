#!/bin/bash
# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

set -euo pipefail

echo "--- ☕ Building Java components"
# shellcheck disable=1091
source /etc/profile.d/enable-gcc-toolset.sh

PATH=/opt/vespa-deps/bin:$PATH

cd "$SOURCE_DIR"

echo "Running Maven build with target: $VESPA_MAVEN_TARGET"
read -ra MVN_EXTRA_OPTS <<< "$VESPA_MAVEN_EXTRA_OPTS"
./mvnw -T "$NUM_MVN_THREADS" "${MVN_EXTRA_OPTS[@]}" "$VESPA_MAVEN_TARGET"
