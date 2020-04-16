#include <flathash.hpp>

namespace flathash {

Flathash::Flathash() {
    number = 6;
}

int Flathash::get_number() const {
  return number;
}

}
