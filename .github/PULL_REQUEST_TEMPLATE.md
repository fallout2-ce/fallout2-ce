### Description

<!--- Clearly describe the changes introduced by this PR. What was fixed, added, or updated? --->

### Reproduction Steps and Savefile (if available)

<!--- Provide detailed steps to reproduce the issue. Include a savefile if applicable, preferrably for Sonora --->

### Screenshots

<!--- If the PR includes visual changes, add screenshots here. --->


<!--- 

**Important Notes** 

- *Preview builds are not available for pull requests from forked repositories.* To enable preview builds, submit the PR from a branch within this repository.

- **Formatting:** CI and auto-format use **clang-format 18** (`silkeh/clang:18`). The repo `.clang-format` is a full explicit config for that version. Do not rely on the system `clang-format` (Ubuntu often ships clang 14, which formats differently). Format locally with:

  ```bash
  ./fix_formatting.sh
  ```

  Or:

  ```bash
  docker run --rm \
    -v "$(pwd)":/app --workdir /app \
    --user "$(id -u):$(id -g)" silkeh/clang:18 \
    bash -c 'find src -type f \( -name "*.cc" -o -name "*.h" \) -print0 | xargs -0 clang-format -i'
  ```

--->

