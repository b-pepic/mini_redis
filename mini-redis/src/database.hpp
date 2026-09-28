#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "store.hpp"

// Database povezuje sve dijelove:
//   - izvrsava naredbe (SET, GET, DEL...) nad Store-om
//   - pazi da samo jedan klijent u isto vrijeme mijenja podatke (mutex)
//   - sprema promjene na disk u AOF datoteku i ucitava ih pri pokretanju
class Database {
public:
    // aof_path = datoteka za spremanje podataka.
    // Prazan string znaci "ne spremaj nista na disk" (korisno za testove).
    explicit Database(const std::string& aof_path = "");

    // Izvrsi naredbu i vrati odgovor koji saljemo klijentu.
    //   {"SET", "ime", "Ivan"} -> "OK"
    //   {"GET", "ime"}         -> "Ivan"
    std::string execute(const std::vector<std::string>& args);

private:
    // Stvarno izvrsavanje naredbe. Poziva se samo dok je mutex zakljucan.
    // log_to_aof = treba li naredbu zapisati u AOF datoteku.
    std::string run(const std::vector<std::string>& args, bool log_to_aof);

    // Procitaj AOF datoteku i ponovno izvrsi sve naredbe iz nje.
    void load_aof(const std::string& path);

    // Dopisi jedan redak na kraj AOF datoteke.
    void append_to_aof(const std::string& line);

    Store store_;
    std::mutex mutex_;     // "lokot" koji stiti store_ i aof_file_
    std::ofstream aof_file_;
};
