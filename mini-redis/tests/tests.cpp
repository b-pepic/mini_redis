// Testovi za Mini Redis.
// Koristimo vlastiti mali "framework" (makro CHECK), da ne treba
// instalirati nikakve dodatne biblioteke.
//
// Testiramo Store, parser i Database BEZ mreze. To je moguce jer
// ti dijelovi ne znaju nista o socketima.

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "database.hpp"
#include "parser.hpp"
#include "store.hpp"

static int passed = 0;
static int failed = 0;

// Ako uvjet nije ispunjen, ispisi gdje je test pao.
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (cond) {                                                          \
            passed++;                                                        \
        } else {                                                             \
            failed++;                                                        \
            std::cout << "  PAO: " << #cond << "  (linija " << __LINE__ << ")\n"; \
        }                                                                    \
    } while (0)

using Args = std::vector<std::string>;

// ---------------- Parser ----------------

void test_parser() {
    std::cout << "Parser...\n";
    CHECK((parse_command("SET ime Ivan") == Args{"SET", "ime", "Ivan"}));
    CHECK((parse_command("  GET    ime  ") == Args{"GET", "ime"}));
    CHECK((parse_command("GET ime\r") == Args{"GET", "ime"}));  // Windows kraj retka
    CHECK(parse_command("").empty());
    CHECK(parse_command("    ").empty());
    CHECK(to_upper("sEt") == "SET");
}

// ---------------- Store ----------------

void test_store_basic() {
    std::cout << "Store: osnovno...\n";
    Store s;
    CHECK(!s.get("a").has_value());
    s.set("a", "1");
    CHECK(s.get("a").value() == "1");
    s.set("a", "2");                     // prepisivanje
    CHECK(s.get("a").value() == "2");
    CHECK(s.exists("a"));
    CHECK(s.del("a"));
    CHECK(!s.del("a"));                  // drugi put nema sto obrisati
    CHECK(!s.exists("a"));
}

void test_store_ttl() {
    std::cout << "Store: istek (TTL)...\n";
    Store s;
    CHECK(s.ttl("nema") == -2);
    s.set("k", "v");
    CHECK(s.ttl("k") == -1);

    CHECK(s.expire_at("k", Store::now_ms() + 10000));
    CHECK(s.ttl("k") == 10);

    s.set("k", "novo");                  // SET brise istek
    CHECK(s.ttl("k") == -1);

    CHECK(s.expire_at("k", Store::now_ms() - 1));  // vec istekao
    CHECK(!s.get("k").has_value());
    CHECK(!s.exists("k"));
    CHECK(!s.expire_at("nema", 0));      // ne mozes postaviti istek nepostojecem kljucu
}

void test_store_keys() {
    std::cout << "Store: KEYS...\n";
    Store s;
    CHECK(s.keys().empty());
    s.set("b", "2");
    s.set("a", "1");
    s.set("c", "3");
    s.expire_at("c", Store::now_ms() - 1);  // istekao, ne smije se pojaviti
    CHECK((s.keys() == Args{"a", "b"}));
}

// ---------------- Database (naredbe) ----------------

void test_commands() {
    std::cout << "Database: naredbe...\n";
    Database db;  // bez AOF datoteke
    CHECK(db.execute({"PING"}) == "PONG");
    CHECK(db.execute({"SET", "ime", "Ivan"}) == "OK");
    CHECK(db.execute({"GET", "ime"}) == "Ivan");
    CHECK(db.execute({"get", "ime"}) == "Ivan");         // mala slova rade
    CHECK(db.execute({"GET", "nema"}) == "(nil)");
    CHECK(db.execute({"EXISTS", "ime"}) == "1");
    CHECK(db.execute({"KEYS"}) == "ime");
    CHECK(db.execute({"DEL", "ime"}) == "1");
    CHECK(db.execute({"DEL", "ime"}) == "0");
    CHECK(db.execute({"KEYS"}) == "(empty)");
}

void test_command_errors() {
    std::cout << "Database: greske...\n";
    Database db;
    CHECK(db.execute({"LETI"}).rfind("ERR", 0) == 0);           // nepoznata naredba
    CHECK(db.execute({"SET", "samo_kljuc"}).rfind("ERR", 0) == 0);
    CHECK(db.execute({"GET"}).rfind("ERR", 0) == 0);
    db.execute({"SET", "k", "v"});
    CHECK(db.execute({"EXPIRE", "k", "abc"}).rfind("ERR", 0) == 0);
}

void test_expire_command() {
    std::cout << "Database: EXPIRE (ceka ~1 s)...\n";
    Database db;
    db.execute({"SET", "k", "v"});
    CHECK(db.execute({"EXPIRE", "k", "1"}) == "1");
    CHECK(db.execute({"EXPIRE", "nema", "1"}) == "0");
    CHECK(db.execute({"TTL", "k"}) == "1");
    CHECK(db.execute({"GET", "k"}) == "v");

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    CHECK(db.execute({"GET", "k"}) == "(nil)");
    CHECK(db.execute({"TTL", "k"}) == "-2");
}

// ---------------- Spremanje na disk (AOF) ----------------

void test_aof() {
    std::cout << "Database: spremanje na disk...\n";
    const std::string path = "test_data.aof";
    std::remove(path.c_str());  // krenemo od prazne datoteke

    {
        Database db(path);
        db.execute({"SET", "a", "1"});
        db.execute({"SET", "b", "2"});
        db.execute({"SET", "c", "3"});
        db.execute({"DEL", "b"});
        db.execute({"EXPIRE", "c", "100"});
        db.execute({"GET", "a"});           // GET ne mijenja podatke, ne ide u AOF
    }  // ovdje se db unistava = kao da smo ugasili server

    {
        Database db(path);                  // "restart": ucita podatke s diska
        CHECK(db.execute({"GET", "a"}) == "1");
        CHECK(db.execute({"GET", "b"}) == "(nil)");
        CHECK(db.execute({"GET", "c"}) == "3");
        CHECK(db.execute({"TTL", "c"}) == "100");  // istek je sacuvan
    }

    std::remove(path.c_str());
}

// ---------------- Vise threadova ----------------

void test_threads() {
    std::cout << "Database: 8 threadova istovremeno...\n";
    Database db;
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; t++) {
        threads.emplace_back([&db, t] {
            for (int i = 0; i < 1000; i++) {
                std::string key = "t" + std::to_string(t) + "_" + std::to_string(i);
                db.execute({"SET", key, "x"});
            }
        });
    }
    for (auto& th : threads) {
        th.join();  // cekaj da svi threadovi zavrse
    }
    // Ako mutex radi, svih 8 * 1000 kljuceva mora biti tu.
    int count = 0;
    for (int t = 0; t < 8; t++) {
        for (int i = 0; i < 1000; i++) {
            std::string key = "t" + std::to_string(t) + "_" + std::to_string(i);
            if (db.execute({"EXISTS", key}) == "1") count++;
        }
    }
    CHECK(count == 8000);
}

int main() {
    test_parser();
    test_store_basic();
    test_store_ttl();
    test_store_keys();
    test_commands();
    test_command_errors();
    test_expire_command();
    test_aof();
    test_threads();

    std::cout << "\nProslo: " << passed << ", palo: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
