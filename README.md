# zhwkrfsp-macserver
This is the server side for zhwkrfsp on mac. this passes the given path to remote.

### USAGE

#### Compilation

No special libraries needed. Run `make` will do the job. Generated executable is `client`.

#### Use

This side should run after linux side started.

Perform:

```shell
$./client <path> <destination-address> [debug-level]
```

You can safely omit `debug-level`.

For debug purposes, there's two debug levels:

- `detailed`: Log down any operations the client performed.
- `interactive`: Log down everything `detailed` will log, and drop into an interactive debug console before it proceed to perform actual filesystem operations. - *NOT RECOMMENDED* because modern shells will perform HUGE amounts of operations every single keyboard stroke. You will have bad experiences, trust me.

### Known Bugs

