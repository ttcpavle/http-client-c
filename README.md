## HTTP GET client in c using sockets

This is a simple http GET client. It is used to fetch website data similar to curl command.

It works using sockets and openssl for secure connection. Connection defaults to HTTP if https is not specified in URL as an argument. 

## How to run

1. install openssl library (used for https connection): 
```bash
sudo apt-get install -y openssl libssl-dev 
```
or 
```bash
sudo dnf install -y openssl openssl-devel
```
on fedora linux.

2. check `openssl --version`
3. run `make` to compile project
4. run the application: 
```bash
./client <url>
```
 (it is preffered to use url with http or https specified since it defaults to http)
 
5. optionally: use `make clean` to clean the build
