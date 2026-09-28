#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Store je "srce" baze: obicna mapa kljuc -> vrijednost,
// plus podrska za istek kljuceva (TTL).
//
// Store NIJE thread-safe sam po sebi. Zakljucavanje (mutex) radi
// klasa Database, koja je jedina koja poziva Store.
class Store {
public:
    // Spremi vrijednost pod kljuc. Ako je kljuc imao istek, istek se brise.
    void set(const std::string& key, const std::string& value);

    // Vrati vrijednost, ili std::nullopt ako kljuc ne postoji (ili je istekao).
    std::optional<std::string> get(const std::string& key);

    // Obrisi kljuc. Vraca true ako je kljuc postojao.
    bool del(const std::string& key);

    // Postoji li kljuc?
    bool exists(const std::string& key);

    // Postavi trenutak isteka kljuca (u milisekundama od 1.1.1970.).
    // Vraca false ako kljuc ne postoji.
    bool expire_at(const std::string& key, int64_t at_ms);

    // Koliko sekundi kljucu ostaje do isteka.
    //   -2 = kljuc ne postoji
    //   -1 = kljuc postoji, ali nema istek
    int64_t ttl(const std::string& key);

    // Svi (neistekli) kljucevi, abecedno sortirani.
    std::vector<std::string> keys();

    // Trenutno vrijeme u milisekundama od 1.1.1970.
    static int64_t now_ms();

private:
    // Ako je kljucu isteklo vrijeme, obrisi ga.  "lazy expiration":
    // kljuc ne brisemo tocno u trenutku isteka, nego tek kad ga netko dotakne.
    void remove_if_expired(const std::string& key);

    std::unordered_map<std::string, std::string> data_;     // kljuc -> vrijednost
    std::unordered_map<std::string, int64_t> expires_;      // kljuc -> trenutak isteka (ms)
};
