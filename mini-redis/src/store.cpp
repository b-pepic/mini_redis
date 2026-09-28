#include "store.hpp"

#include <algorithm>
#include <chrono>

int64_t Store::now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void Store::remove_if_expired(const std::string& key) {
    auto it = expires_.find(key);
    if (it == expires_.end()) {
        return;  // kljuc nema istek
    }
    if (now_ms() >= it->second) {
        data_.erase(key);
        expires_.erase(it);
    }
}

void Store::set(const std::string& key, const std::string& value) {
    data_[key] = value;
    expires_.erase(key);  // novi SET ponistava stari istek (isto radi i pravi Redis)
}

std::optional<std::string> Store::get(const std::string& key) {
    remove_if_expired(key);
    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool Store::del(const std::string& key) {
    remove_if_expired(key);
    expires_.erase(key);
    return data_.erase(key) > 0;  // erase vraca broj obrisanih elemenata (0 ili 1)
}

bool Store::exists(const std::string& key) {
    remove_if_expired(key);
    return data_.count(key) > 0;
}

bool Store::expire_at(const std::string& key, int64_t at_ms) {
    if (!exists(key)) {
        return false;
    }
    expires_[key] = at_ms;
    return true;
}

int64_t Store::ttl(const std::string& key) {
    if (!exists(key)) {
        return -2;
    }
    auto it = expires_.find(key);
    if (it == expires_.end()) {
        return -1;
    }
    int64_t remaining_ms = it->second - now_ms();
    return (remaining_ms + 500) / 1000;  // zaokruzi na najblizu sekundu
}

std::vector<std::string> Store::keys() {
    // Prvo skupimo istekle kljuceve pa ih obrisemo.
    // (Ne smijemo brisati iz mape dok je prolazimo petljom.)
    std::vector<std::string> expired;
    int64_t now = now_ms();
    for (const auto& [key, at] : expires_) {
        if (now >= at) {
            expired.push_back(key);
        }
    }
    for (const auto& key : expired) {
        data_.erase(key);
        expires_.erase(key);
    }

    std::vector<std::string> result;
    for (const auto& [key, value] : data_) {
        result.push_back(key);
    }
    std::sort(result.begin(), result.end());
    return result;
}
