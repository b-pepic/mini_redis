#include "server.hpp"

#include "parser.hpp"

#include <iostream>
#include <string>
#include <thread>

Server::Server(uint16_t port, Database& db) : port_(port), db_(db) {}

bool Server::run() {
    // 1) socket(): napravi "uticnicu" za TCP
    socket_t listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == BAD_SOCKET) {
        std::cerr << "Greska: ne mogu napraviti socket\n";
        return false;
    }

#ifndef _WIN32
    // Linux: dopusti ponovno koristenje porta odmah nakon gasenja servera
    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif

    // 2) bind(): zakaci socket na adresu 127.0.0.1 i nas port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);                     // htons: broj u "mrezni" redoslijed bajtova
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);    // 127.0.0.1 = samo ovo racunalo
    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "Greska: ne mogu koristiti port " << port_ << " (mozda je zauzet?)\n";
        close_socket(listener);
        return false;
    }

    // 3) listen(): pocni cekati klijente (najvise 16 ih moze cekati u redu)
    if (listen(listener, 16) != 0) {
        std::cerr << "Greska: listen nije uspio\n";
        close_socket(listener);
        return false;
    }

    std::cout << "Server radi na 127.0.0.1:" << port_ << std::endl;

    // 4) accept(): cekaj klijente, jednog po jednog, zauvijek
    int next_id = 1;
    while (true) {
        socket_t client = accept(listener, nullptr, nullptr);  // ceka dok se netko ne spoji
        if (client == BAD_SOCKET) {
            continue;
        }
        int id = next_id++;
        std::cout << "Klijent #" << id << " spojen" << std::endl;

        // Svaki klijent dobije svoj thread. detach() znaci:
        // "neka thread radi sam, ne cekam ga".
        std::thread(&Server::handle_client, this, client, id).detach();
    }
}

void Server::handle_client(socket_t client, int client_id) {
    // TCP je "tok bajtova", ne poruka. Jedan recv() moze vratiti pola naredbe,
    // ili dvije naredbe odjednom. Zato sve sto stigne skupljamo u buffer,
    // a naredbu obradimo tek kad vidimo cijeli redak (znak '\n').
    std::string buffer;
    char chunk[1024];

    while (true) {
        int n = recv(client, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            break;  // 0 = klijent je zatvorio vezu, <0 = greska
        }
        buffer.append(chunk, static_cast<size_t>(n));

        // Obradi sve cijele retke koji su se nakupili u bufferu
        size_t newline;
        while ((newline = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);

            auto args = parse_command(line);
            if (args.empty()) {
                continue;  // prazan redak, preskoci
            }

            if (to_upper(args[0]) == "QUIT") {
                send_all(client, "BYE\n");
                close_socket(client);
                std::cout << "Klijent #" << client_id << " odspojen" << std::endl;
                return;
            }

            std::string response = db_.execute(args);
            if (!send_all(client, response + "\n")) {
                break;
            }
        }

        // Zastita: ako netko salje ogroman redak bez '\n', prekini vezu
        if (buffer.size() > 64 * 1024) {
            send_all(client, "ERR predugacka naredba\n");
            break;
        }
    }

    close_socket(client);
    std::cout << "Klijent #" << client_id << " odspojen" << std::endl;
}
