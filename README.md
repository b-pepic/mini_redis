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
- **Spremanje na disk (AOF):** svaka promjena se dopisuje u `data.aof` i ponovno izvrši pri pokretanju
- **Istek ključeva (TTL):** ključ može automatski nestati nakon zadanog broja sekundi
- **Ispravno čitanje s mreže:** naredbe se skupljaju u buffer, pa radi i kad naredba stigne u dijelovima
- **Testovi:** 49 provjera za parser, pohranu, naredbe, spremanje na disk i rad s više threadova

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

Potreban je C++17 prevoditelj i [CMake](https://cmake.org/download/) 3.14+.

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
cmake -S . -B build
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

## Performanse

U mapi `bench/` je benchmark koji spoji više klijenata na server i svi istovremeno šalju naredbe. Za svaki test mjeri propusnost (naredbi u sekundi) i trajanje pojedine naredbe (p50 = medijan, p99 = 99 % naredbi je brže od te vrijednosti).

Svaki klijent šalje naredbu i čeka odgovor prije sljedeće (bez pipelininga). Po testu se šalje ukupno 100 000 naredbi, ravnomjerno podijeljenih na klijente.

### Pokretanje

```bash
# prevođenje (MSYS2 / MinGW); važno je -O2
g++ -std=c++17 -O2 -D_WIN32_WINNT=0x0601 src/*.cpp -o mini_redis.exe -lws2_32
g++ -std=c++17 -O2 -D_WIN32_WINNT=0x0601 bench/bench.cpp -o mini_redis_bench.exe -lws2_32

# terminal 1: server na posebnom portu i s posebnom AOF datotekom
./mini_redis.exe 7001 bench.aof

# terminal 2: benchmark
./mini_redis_bench.exe 7001
```

S CMakeom se benchmark gradi kao `mini_redis_bench`.

### Rezultati

Windows, 16 logičkih jezgri, Release build, klijenti i server na istom računalu (127.0.0.1).

| Klijenata | SET naredbi/s | SET p50 / p99 (µs) | GET naredbi/s | GET p50 / p99 (µs) |
|---:|---:|---:|---:|---:|
| 1  | 28 633 | 33 / 72     | 41 391  | 23 / 53  |
| 2  | 53 557 | 35 / 79     | 79 532  | 24 / 54  |
| 4  | 68 002 | 54 / 122    | 137 037 | 28 / 38  |
| 8  | 67 364 | 60 / 475    | 203 846 | 39 / 51  |
| 16 | 65 984 | 228 / 1 085 | 255 331 | 58 / 128 |
| 32 | 66 086 | 453 / 2 618 | 258 247 | 71 / 410 |

### Što rezultati pokazuju

- **GET skalira dobro:** s 1 na 16 klijenata propusnost raste oko 6×, do otprilike 255 000 naredbi/s. Iznad toga se više ne povećava, vjerojatno zato što klijenti i server dijele iste jezgre.
- **SET zastane na otprilike 67 000 naredbi/s već kod 4 klijenta**, a kašnjenje dalje raste. GET i SET koriste isti mutex, pa uzrok nije sam mutex, nego to što SET, dok ga drži, piše u `data.aof` i radi `flush()`. Za to vrijeme svi ostali klijenti čekaju.

### Sljedeći korak

Izvući pisanje na disk izvan mutexa: SET samo doda redak u red čekanja, a zaseban thread ga periodički zapiše na disk (slično Redisovoj opciji `appendfsync everysec`). Cijena je to da se u slučaju pada servera mogu izgubiti zadnje milisekunde promjena.

**Nekoliko zanimljivih detalja:**

- **TCP je tok bajtova, a ne poruka.** Jedan `recv()` može vratiti pola naredbe ili dvije naredbe odjednom. Server zato sve skuplja u buffer i obrađuje tek cijele retke (do `\n`).
- **Jedan mutex za podatke i AOF.** Kad bi podaci i datoteka imali svaki svoj lokot, dva klijenta bi mogla promijeniti podatke jednim redoslijedom, a zapisati ih u datoteku drugim. Nakon restarta stanje bi tada bilo drugačije.
- **Lazy expiration.** Istekli ključ se ne briše točno u sekundi isteka, nego tek kad ga netko pokuša pročitati. Isto radi i pravi Redis.
- **EXPIRE se u AOF zapisuje kao točan trenutak** (`PEXPIREAT kljuc <ms>`), a ne kao "za 10 sekundi". Tako se odbrojavanje ne resetira nakon restarta.

## Moguća proširenja

- [RESP protokol](https://redis.io/docs/latest/develop/reference/protocol-spec/), da server radi s pravim `redis-cli`
- Event loop s `poll()`/`epoll` umjesto threada po klijentu
- Sažimanje AOF datoteke (trenutno samo raste)
- Vrijednosti s razmacima (navodnici u parseru)
