#pragma once

// Mali "prijevodni sloj" za mrezu.
// Windows (Winsock) i Linux/macOS (POSIX) imaju gotovo iste funkcije
// za sockete (socket, bind, listen, accept, recv, send), ali s par razlika.
// Sve razlike su skupljene ovdje, da ostatak koda ne mora o njima brinuti.

#include <string>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>

    using socket_t = SOCKET;
    const socket_t BAD_SOCKET = INVALID_SOCKET;

    inline void close_socket(socket_t s) { closesocket(s); }
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>

    using socket_t = int;
    const socket_t BAD_SOCKET = -1;

    inline void close_socket(socket_t s) { close(s); }
#endif

// Windows trazi da se mreza "ukljuci" prije prvog koristenja socketa.
inline bool net_init() {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

inline void net_cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// send() ne garantira da ce poslati sve odjednom; moze poslati samo dio.
// Zato saljemo u petlji dok sve ne ode.
inline bool send_all(socket_t s, const std::string& data) {
#ifdef MSG_NOSIGNAL
    const int flags = MSG_NOSIGNAL;  // Linux: ne rusi program ako je klijent otisao
#else
    const int flags = 0;
#endif
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(s, data.data() + sent, static_cast<int>(data.size() - sent), flags);
        if (n <= 0) {
            return false;  // veza je prekinuta
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}
