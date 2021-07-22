# File Transfer over HTTP

A daemon for encrypted, authenticated, single-stream file transfer using
readily-available tools.

Designed with speed, configurability, and security in mind.


## Example usage

### Running the service

**Sample `stunnel.conf` for running the service over HTTPS (port 443 in this case):**

```
[filthttp]
accept = 443
exec = /usr/sbin/filthttp
execargs = filthttp -g xfer -I -p
```

Only users in the specified group can access the service (`-g xfer`), directory indices are generated (`-I`), and upload directory paths are created if they do not exist (`-p`).


**Sample command line for running the service over HTTP using tcpserver:**

```
tcpserver 0 8080 \
    /usr/sbin/filthttp -g users -G noxfer -a -p 2>&1 \
    |multilog /var/log/xferlog
```
Atomic writes are enabled (`-a`), users in the specified group (`-g users`) can access the service _except_ those who are members of a blacklisted group (`-G noxfer`), upload directory paths are created if they do not exist (`-p`), and logs go to the /var/log/xferlog multilog logging directory.

### Accessing with clients

Web browsers may, of course act as clients, and directory indices are generated in HTML format (yet are easily parsable using `awk` or `sed`).  Command-line clients, however, such as `curl` and `wget`, are recommended over browsers, as they are generally more flexible and scriptable.

To retrieve a file using `wget`, you might try the following:

```
wget --user=bob --password=stuff https://host:8443/path/file.zip
```

Or using `curl`:

```
curl -u bob -f -O https://host:8443/path/file.zip
```

To upload a file using `wget`, to the `/some/path` directory (which will be created if it doesn't exist and `-p` has been passed on the server command line):

```
wget --user=bob --password=stuff -O - --post-file=file.zip \
    https://host:8443/some/path/file.zip
```

Or, using `curl`:

```
curl -u bob -f -T file.zip https://host:8443/some/path/file.zip
```

Note that using `curl --data-binary @file.zip` is broken and should not be used.  It will work acceptably for small files, but it loads the entire file into memory before sending, which is inefficient and will break for larger files.  `curl -T` uses the PUT method, which is accepted by filthttp.

To delete the file and its subdirectories (if empty), you might use:

```
curl -u bob -f -X DELETE \
     https://host:8443/some/path/file.zip \
     https://host:8443/some/path \
     https://host:8443/some
```


## Building and installing

download filthttp (this package)
```bash
$ git clone https://github.com/edelkind/filthttp
$ cd filthttp
```

set up lx\_lib
```bash
$ git clone https://github.com/edelkind/lx_lib
$ (cd lx_lib && make)
```

set up minilib
```bash
$ git clone https://github.com/edelkind/minlib
$ (cd minilib && make)
```

set up structural get\_opts
```bash
$ git clone https://github.com/edelkind/get_opts
$ (cd get_opts && make)
```

build and install filthttp
```bash
$ make
$ cp filthttp /usr/sbin/                 # or wherever
$ cp filthttp.1 /usr/share/man/man1      # or wherever
```


## Available options

```bash
$ man filthttp
```


SEE ALSO:

- [`filthttp(1)`](https://edelkind.github.io/filthttp/filthttp.1.html) (also viewable by typing `nroff -man <filthttp.1` in the
  source directory).
- [DESIGN](DESIGN) (in the source directory)
