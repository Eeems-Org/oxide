## Lockscreen hook

This application hooks xochitl to allow running executables at various points of the lifecycle. The lifecycle hook executables all live in `/home/root/.local/share/lockscreen_hook.d/`. The possible hooks are:

- `setup` Runs before starting xochitl
- `hook` Runs during xochtil startup after the passcode handler has been hooked, but before the device is unlocked.
- `unlock` Runs after the device is unlocked, if successful xochitl will be stopped.
- `success` Runs after xochitl exits with exit code 0.
- `fail` Runs if xochitl had a non-zero, non-signal exit code
- `stop` Runs if xochitl's exit code is from SIGINT, SIGKILL, or SIGTERM.
- `crash` Runs if xochitl's exit code is from a signal other than SIGINT, SIGKILL, or SIGTERM.

If any of these hooks don't exist, are not a file, or are not executable, they will be skipped. Otherwise the hook's exit code determines what happens:

- `setup` Non-zero exit code will result in xochitl being started without the the preload.
- `hook` Non-zero exit code will result in the preload disabling itself.
- `unlock` Non-zero exit code will result in xochitl continuing to run normally after unlock. If this hook doesn't exist, isn't a file, or isn't executable, it will be treated as a Non-zero status.
- `success` Non-zero exit code will be used as exit code.
- `fail` Non-zero exit code will be used as exit code.
- `stop` Non-zero exit code will be used as exit code.
- `crash` Non-zero exit code will be used as exit code.
