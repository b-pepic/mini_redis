#pragma once

#include <condition_variable>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "store.hpp"

// Database povezuje sve dijelove:
//   - izvrsava naredbe (SET, GET, DEL...) nad Store-om
//   - pazi da samo jedan klijent u isto vrijeme mijenja podatke (mutex)
//   - sprema promjene na disk u AOF datoteku i ucitava ih pri pokretanju
//
// Pisanje u AOF: SET/DEL/... samo dopisu redak u buffer datoteke (brzo).
// Zaseban "flusher" thread jednom u sekundi posalje buffer operacijskom sustavu.
// Tako klijenti ne cekaju disk kod svake naredbe. Cijena: ako se proces srusi
// (ili ga ugasimo s Ctrl+C), mogu se izgubiti promjene iz zadnje ~1 sekunde.
// Isto radi i pravi Redis s opcijom "appendfsync everysec".
class Database {
public:
    // aof_path = datoteka za spremanje podataka.
    // Prazan string znaci "ne spremaj nista na disk" (korisno za testove).
    explicit Database(const std::string& aof_path = "");

    // Zaustavi flusher thread i zapisi sve sto je ostalo u bufferu.
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

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

    // Dopisi jedan redak na kraj AOF datoteke (samo u buffer, bez flusha).
    void append_to_aof(const std::string& line);

    // Petlja flusher threada: svake sekunde flush AOF datoteke.
    void flush_loop();

    Store store_;
    std::mutex mutex_;     // "lokot" koji stiti store_, aof_file_ i running_
    std::ofstream aof_file_;

    std::condition_variable flush_cv_;  // budi flusher kad ga gasimo, da ne ceka cijelu sekundu
    bool running_ = false;              // radi li flusher (stiti ga mutex_)
    std::thread flusher_;               // mora biti zadnji: pokrece se kad je sve ostalo spremno
};
