# LAN Pingpong

A real-time, two-player Pong-style game that runs over local network (LAN) using TCP sockets. Perfect for quick gaming sessions between connected machines!

## Features

- **Network Play**: One server, one client - play across different machines
- **Real-time Sync**: Game state synchronized continuously between both players
- **Simple Controls**: Easy to learn, fun to play
- **Cross-platform**: Works on POSIX-compatible systems (Linux/macOS)

## Requirements

- POSIX-compatible OS (Linux / macOS)
- C compiler and `make`
- Both machines on the same LAN
- Firewall configured to allow traffic on chosen port

## Build

```bash
make
```
This produces the pingpong executable.

## Usage

Start the Server First

```bash
./pingpong server <port>
```

Example (using port 5789):

```bash
./pingpong server 5789
```
Connect the Client

```bash
./pingpong client <server-ip> <port>
```

Example (connect to server at 192.168.1.5):

```bash
./pingpong client 192.168.1.5 5789
```

Note: If no port is specified, the default port 5789 is used.

## Network Setup

 - Run the server first, then connect with the client

 - Ensure both machines can reach each other over the network

 - Use ifconfig or ip addr to find your server's IP address

 - Make sure the chosen port is open in any firewall settings
