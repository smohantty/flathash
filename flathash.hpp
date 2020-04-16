#pragma once
#if defined _WIN32 || defined __CYGWIN__
  #ifdef BUILDING_FLATHASH
    #define FLATHASH_PUBLIC __declspec(dllexport)
  #else
    #define FLATHASH_PUBLIC __declspec(dllimport)
  #endif
#else
  #ifdef BUILDING_FLATHASH
      #define FLATHASH_PUBLIC __attribute__ ((visibility ("default")))
  #else
      #define FLATHASH_PUBLIC
  #endif
#endif

#include <memory>
#include <cstring>

namespace flathash {

#define SkASSERT(x)
#define SkDEBUGCODE(x)

// Just a pithier wrapper for enable_if_t.
#define SK_WHEN(condition, T) std::enable_if_t<!!(condition), T>

struct SkChecksum {
    /**
     * uint32_t -> uint32_t hash, useful for when you're about to trucate this hash but you
     * suspect its low bits aren't well mixed.
     *
     * This is the Murmur3 finalizer.
     */
    static uint32_t Mix(uint32_t hash) {
        hash ^= hash >> 16;
        hash *= 0x85ebca6b;
        hash ^= hash >> 13;
        hash *= 0xc2b2ae35;
        hash ^= hash >> 16;
        return hash;
    }
};

namespace SkOpts
{
    template <typename T, typename P>
    static inline T sk_unaligned_load(const P* ptr) {
        // TODO: static_assert desirable things about T here so as not to be totally abused.
        T val;
        memcpy(&val, ptr, sizeof(val));
        return val;
    }

    // This is Murmur3.
    /*not static*/ inline uint32_t hash_fn(const void* vdata, size_t bytes, uint32_t hash) {
        auto data = (const uint8_t*)vdata;

        size_t original_bytes = bytes;

        // Handle 4 bytes at a time while possible.
        while (bytes >= 4) {
            uint32_t k = sk_unaligned_load<uint32_t>(data);
            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;

            hash ^= k;
            hash = (hash << 13) | (hash >> 19);
            hash *= 5;
            hash += 0xe6546b64;

            bytes -= 4;
            data  += 4;
        }

        // Handle last 0-3 bytes.
        uint32_t k = 0;
        switch (bytes & 3) {
            case 3: k ^= data[2] << 16;
            case 2: k ^= data[1] <<  8;
            case 1: k ^= data[0] <<  0;
                    k *= 0xcc9e2d51;
                    k = (k << 15) | (k >> 17);
                    k *= 0x1b873593;
                    hash ^= k;
        }

        hash ^= original_bytes;
        return SkChecksum::Mix(hash);
    }
  }

// SkGoodHash should usually be your first choice in hashing data.
// It should be both reasonably fast and high quality.
struct SkGoodHash {
    template <typename K>
    SK_WHEN(sizeof(K) == 4, uint32_t) operator()(const K& k) const {
        return SkChecksum::Mix(*(const uint32_t*)&k);
    }

    template <typename K>
    SK_WHEN(sizeof(K) != 4, uint32_t) operator()(const K& k) const {
        return SkOpts::hash_fn(&k, sizeof(K), 0);
    }

    uint32_t operator()(const char *k) const {
        return SkOpts::hash_fn(k, std::strlen(k), 0);
    }
};


/** Allocate an array of T elements, and free the array in the destructor
 */
template <typename T> class SkAutoTArray  {
public:
    SkAutoTArray() {}
    /** Allocate count number of T elements
     */
    explicit SkAutoTArray(int count) {
        SkASSERT(count >= 0);
        if (count) {
            fArray.reset(new T[count]);
        }
        SkDEBUGCODE(fCount = count;)
    }

    SkAutoTArray(SkAutoTArray&& other) : fArray(std::move(other.fArray)) {
        SkDEBUGCODE(fCount = other.fCount; other.fCount = 0;)
    }
    SkAutoTArray& operator=(SkAutoTArray&& other) {
        if (this != &other) {
            fArray = std::move(other.fArray);
            SkDEBUGCODE(fCount = other.fCount; other.fCount = 0;)
        }
        return *this;
    }

    /** Reallocates given a new count. Reallocation occurs even if new count equals old count.
     */
    void reset(int count) { *this = SkAutoTArray(count);  }

    /** Return the array of T elements. Will be NULL if count == 0
     */
    T* get() const { return fArray.get(); }

    /** Return the nth element in the array
     */
    T&  operator[](int index) const {
        SkASSERT((unsigned)index < (unsigned)fCount);
        return fArray[index];
    }

private:
    std::unique_ptr<T[]> fArray;
    SkDEBUGCODE(int fCount = 0;)
};



// Before trying to use SkTHashTable, look below to see if SkTHashMap or SkTHashSet works for you.
// They're easier to use, usually perform the same, and have fewer sharp edges.

// T and K are treated as ordinary copyable C++ types.
// Traits must have:
//   - static K GetKey(T)
//   - static uint32_t Hash(K)
// If the key is large and stored inside T, you may want to make K a const&.
// Similarly, if T is large you might want it to be a pointer.
template <typename T, typename K, typename Traits = T>
class SkTHashTable {
public:
    SkTHashTable() : fCount(0), fCapacity(0) {}
    SkTHashTable(SkTHashTable&& other)
        : fCount(other.fCount)
        , fCapacity(other.fCapacity)
        , fSlots(std::move(other.fSlots)) { other.fCount = other.fCapacity = 0; }

    SkTHashTable& operator=(SkTHashTable&& other) {
        if (this != &other) {
            this->~SkTHashTable();
            new (this) SkTHashTable(std::move(other));
        }
        return *this;
    }

    // Clear the table.
    void reset() { *this = SkTHashTable(); }

    // How many entries are in the table?
    int count() const { return fCount; }

    // Approximately how many bytes of memory do we use beyond sizeof(*this)?
    size_t approxBytesUsed() const { return fCapacity * sizeof(Slot); }

    // !!!!!!!!!!!!!!!!!                 CAUTION                   !!!!!!!!!!!!!!!!!
    // set(), find() and foreach() all allow mutable access to table entries.
    // If you change an entry so that it no longer has the same key, all hell
    // will break loose.  Do not do that!
    //
    // Please prefer to use SkTHashMap or SkTHashSet, which do not have this danger.

    // The pointers returned by set() and find() are valid only until the next call to set().
    // The pointers you receive in foreach() are only valid for its duration.

    // Copy val into the hash table, returning a pointer to the copy now in the table.
    // If there already is an entry in the table with the same key, we overwrite it.
    T* set(T val) {
        if (4 * fCount >= 3 * fCapacity) {
            this->resize(fCapacity > 0 ? fCapacity * 2 : 4);
        }
        return this->uncheckedSet(std::move(val));
    }

    // If there is an entry in the table with this key, return a pointer to it.  If not, null.
    T* find(const K& key) const {
        uint32_t hash = Hash(key);
        int index = hash & (fCapacity-1);
        for (int n = 0; n < fCapacity; n++) {
            Slot& s = fSlots[index];
            if (s.empty()) {
                return nullptr;
            }
            if (hash == s.hash && key == Traits::GetKey(s.val)) {
                return &s.val;
            }
            index = this->next(index);
        }
        SkASSERT(fCapacity == 0);
        return nullptr;
    }

    // If there is an entry in the table with this key, return it.  If not, null.
    // This only works for pointer type T, and cannot be used to find an nullptr entry.
    T findOrNull(const K& key) const {
        if (T* p = this->find(key)) {
            return *p;
        }
        return nullptr;
    }

    // Remove the value with this key from the hash table.
    void remove(const K& key) {
        SkASSERT(this->find(key));

        uint32_t hash = Hash(key);
        int index = hash & (fCapacity-1);
        for (int n = 0; n < fCapacity; n++) {
            Slot& s = fSlots[index];
            SkASSERT(!s.empty());
            if (hash == s.hash && key == Traits::GetKey(s.val)) {
               this->removeSlot(index);
               return;
            }
            index = this->next(index);
        }
    }

    // Call fn on every entry in the table.  You may mutate the entries, but be very careful.
    template <typename Fn>  // f(T*)
    void foreach(Fn&& fn) {
        for (int i = 0; i < fCapacity; i++) {
            if (!fSlots[i].empty()) {
                fn(&fSlots[i].val);
            }
        }
    }

    // Call fn on every entry in the table.  You may not mutate anything.
    template <typename Fn>  // f(T) or f(const T&)
    void foreach(Fn&& fn) const {
        for (int i = 0; i < fCapacity; i++) {
            if (!fSlots[i].empty()) {
                fn(fSlots[i].val);
            }
        }
    }

    // Call fn on every entry in the table. Fn can return false to remove the entry. You may mutate
    // the entries, but be very careful.
    template <typename Fn>  // f(T*)
    void mutate(Fn&& fn) {
        for (int i = 0; i < fCapacity;) {
            bool keep = true;
            if (!fSlots[i].empty()) {
                keep = fn(&fSlots[i].val);
            }
            if (keep) {
                i++;
            } else {
                this->removeSlot(i);
                // Something may now have moved into slot i, so we'll loop
                // around to check slot i again.
            }
        }
    }

private:
    T* uncheckedSet(T&& val) {
        const K& key = Traits::GetKey(val);
        uint32_t hash = Hash(key);
        int index = hash & (fCapacity-1);
        for (int n = 0; n < fCapacity; n++) {
            Slot& s = fSlots[index];
            if (s.empty()) {
                // New entry.
                s.val  = std::move(val);
                s.hash = hash;
                fCount++;
                return &s.val;
            }
            if (hash == s.hash && key == Traits::GetKey(s.val)) {
                // Overwrite previous entry.
                // Note: this triggers extra copies when adding the same value repeatedly.
                s.val = std::move(val);
                return &s.val;
            }

            index = this->next(index);
        }
        SkASSERT(false);
        return nullptr;
    }

    void resize(int capacity) {
        int oldCapacity = fCapacity;
        SkDEBUGCODE(int oldCount = fCount);

        fCount = 0;
        fCapacity = capacity;
        SkAutoTArray<Slot> oldSlots = std::move(fSlots);
        fSlots = SkAutoTArray<Slot>(capacity);

        for (int i = 0; i < oldCapacity; i++) {
            Slot& s = oldSlots[i];
            if (!s.empty()) {
                this->uncheckedSet(std::move(s.val));
            }
        }
        SkASSERT(fCount == oldCount);
    }

    void removeSlot(int index) {
        fCount--;

        // Rearrange elements to restore the invariants for linear probing.
        for (;;) {
            Slot& emptySlot = fSlots[index];
            int emptyIndex = index;
            int originalIndex;
            // Look for an element that can be moved into the empty slot.
            // If the empty slot is in between where an element landed, and its native slot, then
            // move it to the empty slot. Don't move it if its native slot is in between where
            // the element landed and the empty slot.
            // [native] <= [empty] < [candidate] == GOOD, can move candidate to empty slot
            // [empty] < [native] < [candidate] == BAD, need to leave candidate where it is
            do {
                index = this->next(index);
                Slot& s = fSlots[index];
                if (s.empty()) {
                    // We're done shuffling elements around.  Clear the last empty slot.
                    emptySlot = Slot();
                    return;
                }
                originalIndex = s.hash & (fCapacity - 1);
            } while ((index <= originalIndex && originalIndex < emptyIndex)
                     || (originalIndex < emptyIndex && emptyIndex < index)
                     || (emptyIndex < index && index <= originalIndex));
            // Move the element to the empty slot.
            Slot& moveFrom = fSlots[index];
            emptySlot = std::move(moveFrom);
        }
    }

    int next(int index) const {
        index--;
        if (index < 0) { index += fCapacity; }
        return index;
    }

    static uint32_t Hash(const K& key) {
        uint32_t hash = Traits::Hash(key) & 0xffffffff;
        return hash ? hash : 1;  // We reserve hash 0 to mark empty.
    }

    struct Slot {
        Slot() : val{}, hash(0) {}
        Slot(T&& v, uint32_t h) : val(std::move(v)), hash(h) {}
        Slot(Slot&& o) { *this = std::move(o); }
        Slot& operator=(Slot&& o) {
            val  = std::move(o.val);
            hash = o.hash;
            return *this;
        }

        bool empty() const { return this->hash == 0; }

        T        val;
        uint32_t hash;
    };

    int fCount, fCapacity;
    SkAutoTArray<Slot> fSlots;

    SkTHashTable(const SkTHashTable&) = delete;
    SkTHashTable& operator=(const SkTHashTable&) = delete;
};


// Maps K->V.  A more user-friendly wrapper around SkTHashTable, suitable for most use cases.
// K and V are treated as ordinary copyable C++ types, with no assumed relationship between the two.
template <typename K, typename V, typename HashK = SkGoodHash>
class SkTHashMap {
public:
    SkTHashMap() {}
    SkTHashMap(SkTHashMap&&) = default;
    SkTHashMap& operator=(SkTHashMap&&) = default;

    // Clear the map.
    void reset() { fTable.reset(); }

    // How many key/value pairs are in the table?
    int count() const { return fTable.count(); }

    // Approximately how many bytes of memory do we use beyond sizeof(*this)?
    size_t approxBytesUsed() const { return fTable.approxBytesUsed(); }

    // N.B. The pointers returned by set() and find() are valid only until the next call to set().

    // Set key to val in the table, replacing any previous value with the same key.
    // We copy both key and val, and return a pointer to the value copy now in the table.
    V* set(K key, V val) {
        Pair* out = fTable.set({std::move(key), std::move(val)});
        return &out->val;
    }

    // If there is key/value entry in the table with this key, return a pointer to the value.
    // If not, return null.
    V* find(const K& key) const {
        if (Pair* p = fTable.find(key)) {
            return &p->val;
        }
        return nullptr;
    }

    V& operator[](const K& key) {
        if (V* val = this->find(key)) {
            return *val;
        }
        return *this->set(key, V{});
    }

    // Remove the key/value entry in the table with this key.
    void remove(const K& key) {
        SkASSERT(this->find(key));
        fTable.remove(key);
    }

    // Call fn on every key/value pair in the table.  You may mutate the value but not the key.
    template <typename Fn>  // f(K, V*) or f(const K&, V*)
    void foreach(Fn&& fn) {
        fTable.foreach([&fn](Pair* p){ fn(p->key, &p->val); });
    }

    // Call fn on every key/value pair in the table.  You may not mutate anything.
    template <typename Fn>  // f(K, V), f(const K&, V), f(K, const V&) or f(const K&, const V&).
    void foreach(Fn&& fn) const {
        fTable.foreach([&fn](const Pair& p){ fn(p.key, p.val); });
    }

    // Call fn on every key/value pair in the table. Fn may return false to remove the entry. You
    // may mutate the value but not the key.
    template <typename Fn>  // f(K, V*) or f(const K&, V*)
    void mutate(Fn&& fn) {
        fTable.mutate([&fn](Pair* p) { return fn(p->key, &p->val); });
    }

private:
    struct Pair {
        K key;
        V val;
        static const K& GetKey(const Pair& p) { return p.key; }
        static auto Hash(const K& key) { return HashK()(key); }
    };

    SkTHashTable<Pair, K> fTable;

    SkTHashMap(const SkTHashMap&) = delete;
    SkTHashMap& operator=(const SkTHashMap&) = delete;
};

}

