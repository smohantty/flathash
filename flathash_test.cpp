#include <flathash.hpp>
#include <iostream>

struct static_string
{
	char str[30];
	char value[30];
};

int main(int argc, char **argv) {
    if(argc != 1) {
        std::cout << argv[0] << " takes no arguments.\n";
        return 1;
    }
    std::array<static_string, 10000> db;
    int count {0};
    for (auto &item : db) {
    	snprintf( item.str, 30, "hash key %d",count);
    	snprintf( item.value, 30, "hash value %d",count++);
    }
    flathash::SkTHashMap<const char*, const char *>               map;
    for (const auto &item : db) {
    	map.set(item.str, item.value);
    }
    std::cout<<"Map count = "<<map.count()<<"\n";
    if (map.find(db[50].str))
    {
    	std::cout<<"Map value at key = "<<db[50].str<<*(map.find(db[50].str))<<"\n";
    }
    
    return 0;
}
