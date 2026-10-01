# Shell and CLI conventions

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