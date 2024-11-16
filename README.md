# Legion

A not so minimalistic HTTPS server written in C.

Started out to implement a simple HTTP server (which is still available in branch http_server) to handle GET and HEAD requests. It has now evolved into a HTTPS server, along with a list of optimisations and features mentioned below,

- Using file caching to save small files in the memory directly, for big files maintain a file descriptor from which the file can be read to avoid open and close syscalls everytime someone requests a particular file.

- Implemented threadpools to handle requests, process them and send back a response.

- HTTP server uses sendfile sycall to send the contents of file directly from kernel space instead of copying it to user space and sending in chunks. In case of HTTPS since we need to encrypt the data we need to read chunks in user space encrypt them and then send it to user. There is an equivalent SSL_sendfile which handles encryption in kernel space itself but it requires linux to be compiled with kernel Transport Layer Security, which is not available on all platforms including mine :(

- Supports sending multiple file types 


## Prerequisites


Below libraries are required for compiling from source

- ### Openssl

```
sudo apt-get install libssl-dev
```

This library is required to securely exchange data with a client, by encrypting the the incoming or outgoing data with cryptographic keys. The server's private and public key needs to be signed and recognised by a Certificate Authority.

- ### Brotli
```
sudo apt-get install libbrotli-dev
```

To compress the data using brotli algorithm

- ### Zlib

```
sudo apt install zlib1g-dev
```

Zlib is an open source library for used for data compression to gzip format
