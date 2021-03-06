/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#include <cstring>
#include <errno.h>
#include <iostream>

#include "cppa/config.hpp"
#include "cppa/logging.hpp"
#include "cppa/exception.hpp"
#include "cppa/detail/fd_util.hpp"
#include "cppa/io/tcp_io_stream.hpp"

#ifdef CPPA_WINDOWS
#   include <ws2tcpip.h>
#   include <winsock2.h>
#else
#   include <netdb.h>
#   include <unistd.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#endif

namespace cppa {
namespace io {

using namespace ::cppa::detail::fd_util;

tcp_io_stream::tcp_io_stream(native_socket_type fd) : m_fd(fd) {
    CPPA_LOG_TRACE("new IO stream using socket " << fd);
}

tcp_io_stream::~tcp_io_stream() {
    CPPA_LOG_TRACE("close socket " << m_fd);
    closesocket(m_fd);
}

native_socket_type tcp_io_stream::read_handle() const {
    return m_fd;
}

native_socket_type tcp_io_stream::write_handle() const {
    return m_fd;
}

void tcp_io_stream::read(void* vbuf, size_t len) {
    auto buf = reinterpret_cast<char*>(vbuf);
    size_t rd = 0;
    while (rd < len) {
        auto recv_result = ::recv(m_fd, buf + rd, len - rd, 0);
        handle_read_result(recv_result, true);
        if (recv_result > 0) {
            rd += static_cast<size_t>(recv_result);
        }
        if (rd < len) {
            fd_set rdset;
            FD_ZERO(&rdset);
            FD_SET(m_fd, &rdset);
            if (select(m_fd + 1, &rdset, nullptr, nullptr, nullptr) < 0) {
                throw network_error("select() failed");
            }
        }
    }
}

size_t tcp_io_stream::read_some(void* buf, size_t len) {
    auto recv_result = ::recv(m_fd, reinterpret_cast<char*>(buf), len, 0);
    handle_read_result(recv_result, true);
    return (recv_result > 0) ? static_cast<size_t>(recv_result) : 0;
}

void tcp_io_stream::write(const void* vbuf, size_t len) {
    auto buf = reinterpret_cast<const char*>(vbuf);
    size_t written = 0;
    while (written < len) {
        auto send_result = ::send(m_fd, buf + written, len - written, 0);
        handle_write_result(send_result, true);
        if (send_result > 0) {
            written += static_cast<size_t>(send_result);
        }
        if (written < len) {
            // block until socked is writable again
            fd_set wrset;
            FD_ZERO(&wrset);
            FD_SET(m_fd, &wrset);
            if (select(m_fd + 1, nullptr, &wrset, nullptr, nullptr) < 0) {
                throw network_error("select() failed");
            }
        }
    }
}

size_t tcp_io_stream::write_some(const void* buf, size_t len) {
    CPPA_LOG_TRACE(CPPA_ARG(buf) << ", " << CPPA_ARG(len));
    auto send_result = ::send(m_fd, reinterpret_cast<const char*>(buf), len, 0);
    handle_write_result(send_result, true);
    return static_cast<size_t>(send_result);
}

io::stream_ptr tcp_io_stream::from_sockfd(native_socket_type fd) {
    tcp_nodelay(fd, true);
    nonblocking(fd, true);
    return new tcp_io_stream(fd);
}

io::stream_ptr tcp_io_stream::connect_to(const char* host,
                                          std::uint16_t port) {
    CPPA_LOGF_TRACE(CPPA_ARG(host) << ", " << CPPA_ARG(port));
    CPPA_LOGF_INFO("try to connect to " << host << " on port " << port);
#   ifdef CPPA_WINDOWS
    // make sure TCP has been initialized via WSAStartup
    cppa::get_middleman();
#   endif
    sockaddr_in serv_addr;
    hostent* server;
    native_socket_type fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == invalid_socket) {
        throw network_error("socket creation failed");
    }
    server = gethostbyname(host);
    if (!server) {
        std::string errstr = "no such host: ";
        errstr += host;
        throw network_error(std::move(errstr));
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memmove(&serv_addr.sin_addr.s_addr,
            server->h_addr,
            static_cast<size_t>(server->h_length));
    serv_addr.sin_port = htons(port);
    CPPA_LOGF_DEBUG("call connect()");
    if (connect(fd, (const sockaddr*) &serv_addr, sizeof(serv_addr)) != 0) {
        CPPA_LOGF_ERROR("could not connect to to " << host
                        << " on port " << port);
        throw network_error("could not connect to host");
    }
    CPPA_LOGF_DEBUG("enable nodelay + nonblocking for socket");
    return from_sockfd(fd);
}

} // namespace util
} // namespace cppa

