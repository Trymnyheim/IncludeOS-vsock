#pragma once
#ifndef VSOCK_HPP
#define VSOCK_HPP

#include <cstdint>
#include <stdio.h>

class Vsock {
public:
    Vsocket socket();
    Vsocket socket(int type);



private:
    // List of sockets
};

class Vsocket {
public:
    int bind(vsockAddr addr);

    // backlog is maximum length to which the queue of pending connections for sockfd may grow
    int listen(int backlog);

    /**
     * Accepts the first connection request in the backlog and creates a socket for the 
     * new connection
     */
    Vsocket accept();

    int connect(vsockAddr addr);

    ssize_t send(auto buf, size_t size, int flags);
    ssize_t recv(auto buf, size_t size, int flags);
    // read/write

    int close();
    int shutdown();

    // void setsockopt();

private:

    // Socket state machine (closed, listening, connecting, connected, shutting down)

    // Needs to figure out the credit mechanism
    int type;
    int& backlog;
    vsockAddr bind_addr;
    vsockAddr connect_addr;
};

class vsockAddr {
public: 
    uint32_t cid;
    uint32_t port;
private:
    uint32_t cid() {
        return cid;
    }

    uint32_t port() {
        return port;
    }
    
};

#endif