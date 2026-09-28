#pragma once

#include <cstdint>

#include "database.hpp"
#include "net.hpp"

// Server prima TCP veze od klijenata i prosljeduje njihove naredbe bazi.
// Svaki klijent dobije svoj thread, pa vise klijenata moze raditi istovremeno.
class Server {
public:
    Server(uint16_t port, Database& db);

    // Pokreni server. Ova funkcija se vrti zauvijek (dok ne ugasis program).
    // Vraca false ako se server nije uspio pokrenuti (npr. port je zauzet).
    bool run();

private:
    // Razgovor s jednim klijentom. Radi u zasebnom threadu.
    void handle_client(socket_t client, int client_id);

    uint16_t port_;
    Database& db_;
};
