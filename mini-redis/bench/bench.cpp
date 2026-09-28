// Benchmark za Mini Redis.
// Spoji N klijenata na server, svi istovremeno salju naredbe (SET, pa GET)
// i mjeri se koliko naredbi u sekundi server obradi i koliko traje jedna naredba.
//
// Pokretanje (server mora vec raditi):
//   mini_redis_bench              -> 127.0.0.1:7000, 100000 naredbi po testu
//   mini_redis_bench 7001         -> port 7001
//   mini_redis_bench 7001 200000  -> port 7001, 200000 naredbi po testu

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "../src/net.hpp"

#ifndef _WIN32
    #include <netinet/tcp.h>  // TCP_NODELAY
#endif

using Clock = std::chrono::steady_clock;

// Citaj s veze dok ne dobijemo cijeli redak (isto kao u klijentu).
static bool read_line(socket_t sock, std::string& buffer, std::string& line) {
    char chunk[1024];
    size_t newline;
    while ((newline = buffer.find('\n')) == std::string::npos) {
        int n = recv(sock, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            return false;
        }
        buffer.append(chunk, static_cast<size_t>(n));
    }
    line = buffer.substr(0, newline);
    buffer.erase(0, newline + 1);
    return true;
}

static socket_t connect_to(uint16_t port) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == BAD_SOCKET) return BAD_SOCKET;

    // Iskljuci Nagleov algoritam: saljemo male poruke i zelimo da odmah odu.
    int yes = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket(sock);
        return BAD_SOCKET;
    }
    return sock;
}

struct Result {
    double ops_per_sec = 0;
    double p50_us = 0;   // medijan: pola naredbi je brze od ovoga
    double p99_us = 0;   // 99% naredbi je brze od ovoga
    bool ok = true;
};

// Jedan test: `clients` klijenata, ukupno `total` naredbi, vrsta naredbe `cmd` (SET ili GET).
static Result run_test(uint16_t port, int clients, int total, const std::string& cmd) {
    const int per_client = total / clients;

    // Prvo spojimo sve klijente, jednog po jednog (da ne zatrpamo red cekanja servera).
    std::vector<socket_t> socks;
    for (int c = 0; c < clients; c++) {
        socket_t s = connect_to(port);
        if (s == BAD_SOCKET) {
            std::cerr << "Ne mogu se spojiti na 127.0.0.1:" << port << ". Je li server pokrenut?\n";
            for (socket_t o : socks) close_socket(o);
            return Result{0, 0, 0, false};
        }
        socks.push_back(s);
    }

    std::vector<std::vector<float>> latencies(clients);  // trajanje svake naredbe u mikrosekundama
    std::atomic<bool> go{false};
    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;

    for (int c = 0; c < clients; c++) {
        threads.emplace_back([&, c] {
            auto& lat = latencies[c];
            lat.reserve(per_client);
            std::string buffer, line;
            while (!go.load()) std::this_thread::yield();  // svi krecu u istom trenutku

            for (int i = 0; i < per_client; i++) {
                // Svaki klijent ima svoje kljuceve: k:<klijent>:<i>
                std::string key = "k:" + std::to_string(c) + ":" + std::to_string(i);
                std::string request = (cmd == "SET")
                    ? "SET " + key + " vrijednost_" + std::to_string(i) + "\n"
                    : "GET " + key + "\n";

                auto t0 = Clock::now();
                if (!send_all(socks[c], request) || !read_line(socks[c], buffer, line)) {
                    failed = true;
                    return;
                }
                auto t1 = Clock::now();
                lat.push_back(std::chrono::duration<float, std::micro>(t1 - t0).count());
            }
        });
    }

    auto start = Clock::now();
    go = true;
    for (auto& t : threads) t.join();
    double seconds = std::chrono::duration<double>(Clock::now() - start).count();

    for (socket_t s : socks) {
        send_all(s, "QUIT\n");
        close_socket(s);
    }
    if (failed) return Result{0, 0, 0, false};

    // Spoji sva mjerenja i nadi medijan i 99. percentil
    std::vector<float> all;
    for (auto& l : latencies) all.insert(all.end(), l.begin(), l.end());
    std::sort(all.begin(), all.end());

    Result r;
    r.ops_per_sec = all.size() / seconds;
    r.p50_us = all[all.size() / 2];
    r.p99_us = all[std::min(all.size() - 1, all.size() * 99 / 100)];
    return r;
}

int main(int argc, char* argv[]) {
    uint16_t port = 7000;
    int total = 100000;
    try {
        if (argc >= 2) port = static_cast<uint16_t>(std::stoi(argv[1]));
        if (argc >= 3) total = std::stoi(argv[2]);
    } catch (...) {
        std::cerr << "Upotreba: mini_redis_bench [port] [broj_naredbi]\n";
        return 1;
    }

    if (!net_init()) {
        std::cerr << "Ne mogu pokrenuti mrezu\n";
        return 1;
    }

    const std::vector<int> client_counts = {1, 2, 4, 8, 16, 32};
    std::printf("Mini Redis benchmark: 127.0.0.1:%u, %d naredbi po testu, %u jezgri na racunalu\n\n",
                port, total, std::thread::hardware_concurrency());
    std::printf("%-5s %9s %14s %12s %12s\n", "cmd", "klijenata", "naredbi/s", "p50 (us)", "p99 (us)");
    std::printf("-----------------------------------------------------------\n");

    // SET ide prvi, da GET poslije ima sto citati
    for (const std::string cmd : {"SET", "GET"}) {
        for (int clients : client_counts) {
            Result r = run_test(port, clients, total, cmd);
            if (!r.ok) {
                std::cerr << "Test nije uspio (veza prekinuta).\n";
                net_cleanup();
                return 1;
            }
            std::printf("%-5s %9d %14.0f %12.1f %12.1f\n",
                        cmd.c_str(), clients, r.ops_per_sec, r.p50_us, r.p99_us);
        }
        std::printf("\n");
    }

    net_cleanup();
    return 0;
}
