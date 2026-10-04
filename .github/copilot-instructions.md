# Kronos Project Instructions

## Shell and CLI conventions

- When generating CLI commands in documentation, README files, setup guides, comments, or examples, always use PowerShell syntax.
- Assume commands are executed in PowerShell on Windows unless explicitly stated otherwise.
- Do not use Bash-specific syntax such as:
  - `export VAR=value`
  - `VAR=value command`
  - `$(...)` when a PowerShell equivalent is appropriate
  - `rm -rf`, `cp`, `mv`, `touch`, or other Unix-specific commands
- Use PowerShell equivalents, for example:
  - `$env:VAR = "value"` for environment variables
  - `Remove-Item -Recurse -Force` instead of `rm -rf`
  - `Copy-Item` instead of `cp`
  - `Move-Item` instead of `mv`
  - `New-Item` instead of `touch`
- Commands for cross-platform tools such as `dotnet`, `git`, `docker`, `npm`, and `kubectl` may be used directly, but surrounding shell syntax must be PowerShell-compatible.
- Use PowerShell code fences for shell examples:

  ```powershell
  dotnet build
  ```

## Bug fixing and unexpected behavior

When I report a bug, exception, error, or unexpected behavior:

- Analyze the reported problem and make the code changes necessary to fix it.
- You may inspect relevant source files, configuration, logs, and existing code to determine the cause.
- Do **not** build, compile, run, start, or otherwise execute the application unless I explicitly ask you to do so.
- Do **not** run unit tests, integration tests, end-to-end tests, test suites, or other automated validation unless I explicitly ask you to do so.
- Do not run commands such as `dotnet build`, `dotnet test`, `npm test`, `npm run build`, or equivalent validation commands unless explicitly requested.
- After making the fix, briefly explain:
  - the likely root cause;
  - what was changed;
  - any assumptions made;
  - what I should build or test manually to verify the fix, if applicable.

Treat a bug report itself as authorization to investigate and modify the code, but **not** as authorization to build or test the project.