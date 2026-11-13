// The controller must mirror the current roll even after a drop clears it.
#include <cassert>
#define RESONA_APP_ID "org.resona.demo.hype-dial"
#define RESONA_APP_NAME "Hype Dial"
#define RESONA_APP_VERSION "0.4.7"
#include "../src/app.cpp"

int main() {
    State s = {};
    s.backing = 100;
    bind(&s);
    assert(kBinds == 13);
    assert(kBind[8].cc == CC_BACKING && kBind[8].value == &s.backing);
    assert(ctl::to_cc(kBind[8]) == 85);
    assert(ctl::from_cc(kBind[8], 127) == 150);
    s.rollIdx = 2;
    mirror(&s);
    assert(s.c.roll == 3);
    s.rollIdx = -1;
    mirror(&s);
    assert(s.c.roll == 0);
}
