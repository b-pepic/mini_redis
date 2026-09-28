#include <iostream>
#include <string>

#include "database.hpp"
#include "net.hpp"
#include "server.hpp"

// Pokretanje:
//   mini_redis              -> port 7000, podaci u data.aof
//   mini_redis 8000         -> port 8000
//   mini_redis 8000 moj.aof -> port 8000, podaci u moj.aof
int main(int argc, char* argv[]) {
    uint16_t port = 7000;
    std::string aof_path = "data.aof";

    if (argc >= 2) {
        try {
            int p = std::stoi(argv[1]);
            if (p < 1 || p > 65535) throw std::out_of_range("port");
            port = static_cast<uint16_t>(p);
        } catch (...) {
            std::cerr << "Neispravan port: " << argv[1] << "\n";
            return 1;
        }
    }
    if (argc >= 3) {
        aof_path = argv[2];
    }

    if (!net_init()) {
        std::cerr << "Ne mogu pokrenuti mrezu\n";
        return 1;
    }

    Database db(aof_path);  // ovdje se ucitavaju stari podaci s diska
    Server server(port, db);
    bool ok = server.run();  // vrti se zauvijek

    net_cleanup();
    return ok ? 0 : 1;
}
