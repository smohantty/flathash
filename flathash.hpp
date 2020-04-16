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

namespace flathash {

class FLATHASH_PUBLIC Flathash {

public:
  Flathash();
  int get_number() const;

private:

  int number;

};

}

