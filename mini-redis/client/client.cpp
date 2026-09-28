// Jednostavan klijent za Mini Redis.
// Procita naredbu s tipkovnice, posalje je serveru, ispise odgovor. I tako u krug.
//
// Pokretanje:
//   mini_redis_client          -> spoji se na 127.0.0.1:7000
//   mini_redis_client 8000     -> spoji se na 127.0.0.1:8000

#include <cstdint>
#include <iostream>
#include <string>

#include "../src/net.hpp"

// Citaj s veze dok ne dobijemo cijeli redak (do '\n').
// Visak koji stigne ostaje u bufferu za sljedeci put.
bool read_line(socket_t sock, std::string& buffer, std::string& line) {
    char chunk[1024];
    size_t newline;
    while ((newline = buffer.find('\n')) == std::string::npos) {
        int n = recv(sock, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            return false;  // server je zatvorio vezu
        }
        buffer.append(chunk, static_cast<size_t>(n));
    }
    line = buffer.substr(0, newline);
    buffer.erase(0, newline + 1);
    return true;
}

int main(int argc, char* argv[]) {
    uint16_t port = 7000;
    if (argc >= 2) {
        try {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        } catch (...) {
            std::cerr << "Neispravan port: " << argv[1] << "\n";
            return 1;
        }
    }

    if (!net_init()) {
        std::cerr << "Ne mogu pokrenuti mrezu\n";
        return 1;
    }

    // socket() + connect(): spoji se na server
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (sock == BAD_SOCKET ||
        connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "Ne mogu se spojiti na 127.0.0.1:" << port
                  << ". Je li server pokrenut?\n";
        net_cleanup();
        return 1;
    }

    std::cout << "Spojen na 127.0.0.1:" << port << ". Upisi QUIT za izlaz.\n";

    std::string buffer;
    std::string input;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input)) {
            break;  // kraj unosa (Ctrl+Z na Windowsu, Ctrl+D na Linuxu)
        }
        if (input.find_first_not_of(" \t\r") == std::string::npos) {
            continue;  // prazan redak, ne saljemo nista
        }

        if (!send_all(sock, input + "\n")) {
            std::cerr << "Veza je prekinuta\n";
            break;
        }

        std::string response;
        if (!read_line(sock, buffer, response)) {
            std::cerr << "Server je zatvorio vezu\n";
            break;
        }
        std::cout << response << "\n";

        if (response == "BYE") {
            break;
        }
    }

    close_socket(sock);
    net_cleanup();
    return 0;
}
