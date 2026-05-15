#!/bin/bash

set -euo pipefail

docker run --rm \
  -v "$(pwd)":/app --workdir /app \
  --user "$(id -u):$(id -g)" silkeh/clang:14 \
  bash -c 'find src -type f \( -name "*.cc" -o -name "*.h" \) -print0 | xargs -0 clang-format -i'

