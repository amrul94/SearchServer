#pragma once

#include <map>
#include <mutex>
#include <vector>

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : guard(bucket.mutex),
              ref_to_value(bucket.map[key]){

        }
    };

    explicit ConcurrentMap(size_t bucket_count)
            : maps_(bucket_count) {
    }

    Access operator[](const Key& key) {
        return { key, GetBucket(key) };
    }

    void Erase(const Key& key) {
        Bucket& bucket = GetBucket(key);
        std::lock_guard guard(bucket.mutex);
        bucket.map.erase(key);
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> ordinary_map;
        for (auto& [map_mutex, ord_map] : maps_) {
            std::lock_guard values_guard(map_mutex);
            for (const auto& [key, value] : ord_map) {
                ordinary_map[key] = value;
            }
        }
        return ordinary_map;
    }

private:
    std::vector<Bucket> maps_;

    Bucket& GetBucket(const Key& key) {
        return maps_[static_cast<uint64_t>(key) % maps_.size()];
    }
};