#include "database.hpp"

#include "parser.hpp"

#include <iostream>

namespace {

// Pokusaj pretvoriti tekst u cijeli broj. Vraca false ako tekst nije broj.
bool parse_int(const std::string& text, int64_t& out) {
    try {
        size_t used = 0;
        out = std::stoll(text, &used);
        return used == text.size();  // "12abc" nije ispravan broj
    } catch (...) {
        return false;  // stoll baca iznimku za "abc" ili prevelik broj
    }
}

}  // namespace

Database::Database(const std::string& aof_path) {
    if (aof_path.empty()) {
        return;  // radimo samo u memoriji
    }
    load_aof(aof_path);
    // std::ios::app = otvori za dopisivanje na kraj (ne brisi stari sadrzaj)
    aof_file_.open(aof_path, std::ios::app);
    if (!aof_file_) {
        std::cerr << "Upozorenje: ne mogu otvoriti " << aof_path
                  << ", podaci se nece spremati na disk.\n";
    }
}

void Database::load_aof(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return;  // datoteka jos ne postoji: prvo pokretanje, nema sto ucitati
    }
    int count = 0;
    std::string line;
    while (std::getline(file, line)) {
        auto args = parse_command(line);
        if (!args.empty()) {
            run(args, false);  // false: ne zapisuj ponovno ono sto upravo citamo
            count++;
        }
    }
    std::cout << "Ucitano " << count << " naredbi iz " << path << std::endl;
}

void Database::append_to_aof(const std::string& line) {
    if (aof_file_.is_open()) {
        aof_file_ << line << "\n";
        aof_file_.flush();  // odmah zapisi na disk, ne cekaj
    }
}

std::string Database::execute(const std::vector<std::string>& args) {
    // lock_guard zakljuca mutex sada, a automatski ga otkljuca kad funkcija zavrsi.
    // Dok je zakljucan, nijedan drugi klijent (thread) ne moze uci ovdje.
    std::lock_guard<std::mutex> lock(mutex_);
    return run(args, true);
}

std::string Database::run(const std::vector<std::string>& args, bool log_to_aof) {
    if (args.empty()) {
        return "ERR prazna naredba";
    }

    const std::string cmd = to_upper(args[0]);
    const std::string wrong_args = "ERR krivi broj argumenata za " + cmd;

    // PING -> PONG (provjera je li server ziv)
    if (cmd == "PING") {
        return "PONG";
    }

    // SET kljuc vrijednost
    if (cmd == "SET") {
        if (args.size() != 3) return wrong_args;
        store_.set(args[1], args[2]);
        if (log_to_aof) append_to_aof("SET " + args[1] + " " + args[2]);
        return "OK";
    }

    // GET kljuc
    if (cmd == "GET") {
        if (args.size() != 2) return wrong_args;
        auto value = store_.get(args[1]);
        return value ? *value : "(nil)";
    }

    // DEL kljuc -> 1 ako je obrisan, 0 ako nije postojao
    if (cmd == "DEL") {
        if (args.size() != 2) return wrong_args;
        bool removed = store_.del(args[1]);
        if (removed && log_to_aof) append_to_aof("DEL " + args[1]);
        return removed ? "1" : "0";
    }

    // EXISTS kljuc -> 1 ili 0
    if (cmd == "EXISTS") {
        if (args.size() != 2) return wrong_args;
        return store_.exists(args[1]) ? "1" : "0";
    }

    // EXPIRE kljuc sekunde -> kljuc nestaje za toliko sekundi
    if (cmd == "EXPIRE") {
        if (args.size() != 3) return wrong_args;
        int64_t seconds;
        if (!parse_int(args[2], seconds)) return "ERR vrijeme mora biti cijeli broj";

        int64_t at_ms = Store::now_ms() + seconds * 1000;
        if (!store_.expire_at(args[1], at_ms)) return "0";

        // U AOF NE pisemo "EXPIRE kljuc 10", nego tocan trenutak isteka.
        // Inace bi se nakon restarta servera odbrojavanje krenulo ispocetka.
        if (log_to_aof) append_to_aof("PEXPIREAT " + args[1] + " " + std::to_string(at_ms));
        return "1";
    }

    // PEXPIREAT kljuc trenutak_u_ms -> uglavnom se koristi pri ucitavanju AOF-a
    if (cmd == "PEXPIREAT") {
        if (args.size() != 3) return wrong_args;
        int64_t at_ms;
        if (!parse_int(args[2], at_ms)) return "ERR vrijeme mora biti cijeli broj";
        if (!store_.expire_at(args[1], at_ms)) return "0";
        if (log_to_aof) append_to_aof("PEXPIREAT " + args[1] + " " + args[2]);
        return "1";
    }

    // TTL kljuc -> sekunde do isteka, -1 = nema istek, -2 = ne postoji
    if (cmd == "TTL") {
        if (args.size() != 2) return wrong_args;
        return std::to_string(store_.ttl(args[1]));
    }

    // KEYS -> svi kljucevi u jednom retku, odvojeni razmakom
    if (cmd == "KEYS") {
        if (args.size() != 1) return wrong_args;
        auto keys = store_.keys();
        if (keys.empty()) return "(empty)";
        std::string result;
        for (size_t i = 0; i < keys.size(); i++) {
            if (i > 0) result += " ";
            result += keys[i];
        }
        return result;
    }

    return "ERR nepoznata naredba " + args[0];
}
