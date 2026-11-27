# Privileged commands

- Never run `sudo` or start password/fingerprint authentication prompts.
- The user runs privileged host commands manually in a dedicated terminal.
  Provide the exact command and its purpose, then wait for the user to confirm
  completion before continuing work that depends on it.
- Run Mock as the regular user through `/usr/bin/mock`; never use `sudo mock`.
- SD-card flashing is manual: provide `sudo dd` instructions for the device
  selected by the user, and never execute the flash command.
