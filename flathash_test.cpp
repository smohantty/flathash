#include <flathash.hpp>
#include <iostream>
#include <unordered_map>

struct DbEntry
{
	char str[30];
	char value[30];
};

template <int TestSize>
class Setup
{
public:
    explicit Setup()
    {
        int count = 0;
        for (auto &item : db) {
            snprintf( item.str, 30, "hash key %d",count);
            snprintf( item.value, 30, "hash value %d",count++);
        }
    }

    void flatHashTest()
    {
        for (const auto &item : db) {
            flatHash.set(item.str, item.value);
        }

        std::cout<<"flatHash count = "<<flatHash.count()<<"\n";
    }

    void unordered_map_test()
    {
        for (const auto &item : db) {
            unoder_map[item.str] = item.value;
        }

        std::cout<<"unordered_map_test count = "<<unoder_map.size()<<"\n";
    }    
private:
   std::array<DbEntry, TestSize> db;
   flathash::SkTHashMap<const char*, const char *>  flatHash;
   std::unordered_map<const char*, const char *>  unoder_map; 
};

int main(int argc, char **argv) {
    Setup<10000> tc;

    tc.flatHashTest();

    tc.unordered_map_test();

    std::cin.ignore();
    return 0;
}
