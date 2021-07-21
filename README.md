# File Transfer over HTTP

A daemon for encrypted, authenticated, single-stream file transfer using
readily-available tools.

Designed with speed, configurability, and security in mind.

## Building and installing

download filthttp (this package)
```bash
$ git clone http://github.com/edelkind/filthttp
$ cd filthttp
```

set up lx\_lib
```bash
$ git clone http://github.com/edelkind/lx_lib
$ (cd lx_lib && make)
```

set up minilib
```bash
$ git clone http://github.com/edelkind/minlib
$ (cd minilib && make)
```

set up structural get\_opts
```bash
$ git clone http://github.com/edelkind/get_opts
$ (cd get_opts && make)
```

build and install filthttp
```bash
$ make
$ cp filthttp /usr/sbin/                 # or wherever
$ cp filthttp.1 /usr/share/man/man1      # or wherever
```


## Running

```bash
$ man filthttp
```


SEE ALSO:

- [`filthttp(1)`](https://edelkind.github.io/filthttp/filthttp.1.html) (also viewable by typing `nroff -man <filthttp.1` in the
  source directory).
- [DESIGN](DESIGN) (in the source directory)
