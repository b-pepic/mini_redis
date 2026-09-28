# Mini Redis

Pojednostavljena verzija [Redisa](https://redis.io) napisana u C++17: key-value baza podataka koja radi kao TCP server. Klijenti se spajaju preko mreže i šalju naredbe (`SET`, `GET`, `DEL`...), a server podatke drži u memoriji i sprema ih na disk da prežive restart.

Radi na **Windowsu**, **Linuxu** i **macOS-u**, bez vanjskih biblioteka.

```
> SET ime Ivan
OK
> GET ime
Ivan
> EXPIRE ime 10
1
> TTL ime
10
> KEYS
ime
```

## Značajke

- **TCP server:** sluša na portu (zadano `7000`) i prima naredbe u obliku teksta, jedna naredba po retku
- **Više klijenata istovremeno:** svaki klijent radi u svom threadu, a pristup podacima štiti `std::mutex`
- **Spremanje na disk (AOF):** svaka promjena se dopisuje u `data.aof` i ponovno izvrši pri pokretanju; zaseban thread jednom u sekundi šalje buffer na disk, pa klijenti ne čekaju disk
- **Istek ključeva (TTL):** ključ može automatski nestati nakon zadanog broja sekundi
- **Ispravno čitanje s mreže:** naredbe se skupljaju u buffer, pa radi i kad naredba stigne u dijelovima
- **Testovi:** 49 provjera za parser, pohranu, naredbe, spremanje na disk i rad s više threadova
- **Benchmark:** vlastiti alat za mjerenje opterećenja; SET je nakon optimizacije ~2,5× brži (vidi [Performanse](#performanse))

## Naredbe

| Naredba | Opis | Odgovor |
|---|---|---|
| `PING` | Provjera je li server živ | `PONG` |
| `SET kljuc vrijednost` | Spremi vrijednost | `OK` |
| `GET kljuc` | Dohvati vrijednost | vrijednost ili `(nil)` |
| `DEL kljuc` | Obriši ključ | `1` obrisan, `0` nije postojao |
| `EXISTS kljuc` | Postoji li ključ | `1` ili `0` |
| `EXPIRE kljuc sekunde` | Ključ nestaje za N sekundi | `1` ili `0` ako ključ ne postoji |
| `TTL kljuc` | Koliko sekundi ključu ostaje | broj, `-1` bez isteka, `-2` ne postoji |
| `KEYS` | Popis svih ključeva | ključevi odvojeni razmakom ili `(empty)` |
| `QUIT` | Prekini vezu | `BYE` |

Naredbe nisu osjetljive na velika i mala slova (`set` = `SET`). Ključevi i vrijednosti ne smiju sadržavati razmake.

## Build i pokretanje

Potreban je C++17 prevoditelj (npr. g++ iz [MSYS2](https://www.msys2.org/) na Windowsu). CMake nije obavezan.

### Windows (g++ / MSYS2, može i iz terminala u VS Codeu)

Sve naredbe se pokreću iz mape projekta (one u kojoj su `src`, `client`, `bench`...).

```bash
# prevođenje: server, klijent i benchmark (-O2 = optimizirano)
g++ -std=c++17 -O2 -D_WIN32_WINNT=0x0601 src/main.cpp src/server.cpp src/store.cpp src/parser.cpp src/database.cpp -o mini_redis.exe -lws2_32
g++ -std=c++17 -O2 -D_WIN32_WINNT=0x0601 client/client.cpp -o mini_redis_client.exe -lws2_32
g++ -std=c++17 -O2 -D_WIN32_WINNT=0x0601 bench/bench.cpp -o mini_redis_bench.exe -lws2_32

# terminal 1: server
./mini_redis.exe

# terminal 2: klijent
./mini_redis_client.exe
```

U VS Codeu drugi terminal otvoriš gumbom *Split Terminal*. Server se gasi s Ctrl+C.

Testovi:

```bash
g++ -std=c++17 -O2 -Isrc src/store.cpp src/parser.cpp src/database.cpp tests/tests.cpp -o tests.exe
./tests.exe
```

S CMakeom (3.14+) projekt se može graditi i na načine opisane dolje.

### Windows (Visual Studio)

1. U Visual Studiju odaberi **File → Open → Folder** i otvori mapu projekta. VS sam prepozna `CMakeLists.txt`.
2. Gore u izborniku za pokretanje odaberi `mini_redis.exe` i pokreni.
3. Zatim odaberi `mini_redis_client.exe` i pokreni i njega (ili ga pokreni iz terminala, vidi dolje).

Ili iz terminala (Developer PowerShell ili obični terminal ako je CMake instaliran):

```powershell
cmake -S . -B build
cmake --build build --config Release

# terminal 1: server
build\Release\mini_redis.exe

# terminal 2: klijent
build\Release\mini_redis_client.exe
```

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/mini_redis             # terminal 1: server
./build/mini_redis_client      # terminal 2: klijent
```

Na Linuxu/macOS-u umjesto našeg klijenta možeš koristiti i `nc 127.0.0.1 7000`.

### Opcije servera

```
mini_redis                  port 7000, podaci u data.aof
mini_redis 8000             port 8000
mini_redis 8000 moj.aof     port 8000, podaci u moj.aof
```

Klijent prima port kao argument: `mini_redis_client 8000`.

### Testovi

```bash
ctest --test-dir build -C Release --output-on-failure
# ili izravno: build/tests  (na Windowsu build\Release\tests.exe)
```

## Kako radi

```
  klijent                                 SERVER
┌──────────┐  "SET ime Ivan\n"  ┌────────────────────────────────────┐
│          │ ─────────────────► │ server.cpp   socketi, thread po    │
│          │                    │              klijentu, buffer      │
│          │                    │      │                             │
│          │                    │      ▼                             │
│          │                    │ parser.cpp   "SET ime Ivan"        │
│          │                    │              → {"SET","ime","Ivan"}│
│          │                    │      │                             │
│          │                    │      ▼                             │
│          │      "OK\n"        │ database.cpp izvrši naredbu (mutex)│
│          │ ◄───────────────── │      │              │              │
└──────────┘                    │      ▼              ▼              │
                                │ store.cpp       data.aof (disk)    │
                                │ mapa + TTL                         │
                                └────────────────────────────────────┘
```

| Datoteka | Uloga |
|---|---|
| `src/store.*` | Podaci: `unordered_map` ključ → vrijednost i druga mapa za trenutke isteka |
| `src/parser.*` | Rastavlja redak teksta na riječi |
| `src/database.*` | Izvršava naredbe, zaključava mutex, piše i čita AOF datoteku |
| `src/server.*` | TCP server: `socket` → `bind` → `listen` → `accept`, thread po klijentu |
| `src/net.hpp` | Skriva razlike između Windows (Winsock) i POSIX socketa |
| `src/main.cpp` | Čita argumente i pokreće server |
| `client/client.cpp` | Klijent za terminal |
| `tests/tests.cpp` | Testovi (bez mreže, jer jezgra ne ovisi o socketima) |
| `bench/bench.cpp` | Benchmark: više klijenata istovremeno, mjeri propusnost i kašnjenje |

**Nekoliko zanimljivih detalja:**

- **TCP je tok bajtova, a ne poruka.** Jedan `recv()` može vratiti pola naredbe ili dvije naredbe odjednom. Server zato sve skuplja u buffer i obrađuje tek cijele retke (do `\n`).
- **Jedan mutex za podatke i AOF.** Kad bi podaci i datoteka imali svaki svoj lokot, dva klijenta bi mogla promijeniti podatke jednim redoslijedom, a zapisati ih u datoteku drugim. Nakon restarta stanje bi tada bilo drugačije.
- **Lazy expiration.** Istekli ključ se ne briše točno u sekundi isteka, nego tek kad ga netko pokuša pročitati. Isto radi i pravi Redis.
- **AOF se flusha jednom u sekundi.** Naredba samo dopiše redak u buffer datoteke, a zaseban thread ga svake sekunde pošalje operacijskom sustavu. Thread čeka na `std::condition_variable`, pa se pri gašenju odmah probudi i napravi zadnji flush. Cijena: ako se server sruši ili ga ugasimo s Ctrl+C, mogu se izgubiti promjene iz zadnje ~1 sekunde (isto kao Redisova opcija `appendfsync everysec`).
- **EXPIRE se u AOF zapisuje kao točan trenutak** (`PEXPIREAT kljuc <ms>`), a ne kao "za 10 sekundi". Tako se odbrojavanje ne resetira nakon restarta.

## Performanse

`bench/bench.cpp` spoji 1–32 klijenta na server i svi istovremeno šalju naredbe. Svaki klijent šalje naredbu i čeka odgovor prije sljedeće (bez pipelininga). Po testu se šalje ukupno 100 000 naredbi, ravnomjerno podijeljenih na klijente. Mjeri se propusnost (naredbi u sekundi) i trajanje naredbe: p50 je medijan, a p99 vrijednost od koje je 99 % naredbi brže.

### Pokretanje

```bash
# terminal 1: server na posebnom portu i s posebnom AOF datotekom, da benchmark ne napuni data.aof
./mini_redis.exe 7001 bench.aof

# terminal 2
./mini_redis_bench.exe 7001        # opcionalno: ./mini_redis_bench.exe 7001 200000
```

Nakon mjerenja ugasi server (Ctrl+C) i obriši `bench.aof`.

### Rezultati

Windows, 16 logičkih jezgri, g++ `-O2`, klijenti i server na istom računalu (127.0.0.1).

**Prije optimizacije** (`flush()` nakon svake naredbe koja mijenja podatke):

| Klijenata | SET naredbi/s | SET p50 / p99 (µs) | GET naredbi/s | GET p50 / p99 (µs) |
|---:|---:|---:|---:|---:|
| 1  | 28 633 | 33 / 72     | 41 391  | 23 / 53  |
| 2  | 53 557 | 35 / 79     | 79 532  | 24 / 54  |
| 4  | 68 002 | 54 / 122    | 137 037 | 28 / 38  |
| 8  | 67 364 | 60 / 475    | 203 846 | 39 / 51  |
| 16 | 65 984 | 228 / 1 085 | 255 331 | 58 / 128 |
| 32 | 66 086 | 453 / 2 618 | 258 247 | 71 / 410 |

**Nakon optimizacije** (flush jednom u sekundi, u zasebnom threadu):

| Klijenata | SET naredbi/s | SET p50 / p99 (µs) | GET naredbi/s | GET p50 / p99 (µs) |
|---:|---:|---:|---:|---:|
| 1  | 37 551  | 23 / 69    | 40 494  | 23 / 72  |
| 2  | 67 353  | 26 / 68    | 72 720  | 25 / 69  |
| 4  | 111 925 | 31 / 63    | 131 666 | 29 / 41  |
| 8  | 146 806 | 43 / 105   | 180 163 | 42 / 60  |
| 16 | 160 731 | 94 / 312   | 170 328 | 48 / 100 |
| 32 | 162 569 | 209 / 732  | 223 602 | 76 / 221 |

### Što rezultati pokazuju

- **Prije:** GET je rastao do ~255 000 naredbi/s, a SET je već kod 4 klijenta zapeo na ~67 000/s, uz p99 do 2,6 ms. Obje naredbe koriste isti mutex, pa uzrok nije sam mutex, nego to što je SET dok ga drži radio `flush()` na datoteku. Za to vrijeme svi ostali klijenti su čekali.
- **Provjera hipoteze:** samo uklanjanje `flush()` (kao eksperiment) podiglo je SET na ~166 000/s. Time je potvrđeno da je flush usko grlo.
- **Nakon:** SET s 32 klijenta obradi ~163 000 naredbi/s (**~2,5× više**), a p99 je pao s 2,6 ms na 0,7 ms.
- SET je i dalje nešto sporiji od GET-a pri većem broju klijenata, jer dok drži mutex radi više posla: umeće novi ključ u hash tablicu (alokacija, povremeno rehash) i dopisuje redak u buffer datoteke.
- Rezultati variraju ±5–10 % između pokretanja.

## Moguća proširenja

- [RESP protokol](https://redis.io/docs/latest/develop/reference/protocol-spec/), da server radi s pravim `redis-cli`
- Event loop s `poll()`/`epoll` umjesto threada po klijentu
- Sažimanje AOF datoteke (trenutno samo raste)
- Vrijednosti s razmacima (navodnici u parseru)
