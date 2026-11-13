// Regression: production firmware sends encoder gestures without raw key edges.
#include <cassert>
#define RESONA_APP_ID "org.resona.demo.hype-dial"
#define RESONA_APP_NAME "Hype Dial"
#define RESONA_APP_VERSION "0.4.7"
#include "../src/app.cpp"

int main() {
    State s = {};
    s.brake = -1;
    s.bothAt = -1;
    s.step = 0;
    s.energy = 2;

    turn(&s, 1, 1);
    click(&s, 1);
    assert(s.stutter == 1); // short press after turning must still trigger

    longPress(&s, 1);
    assert(s.building == 1); // native long gesture starts BUILD

    click(&s, 1);
    assert(s.buildRelease == 1); // next click schedules DROP

    State genre = {};
    genre.brake = -1;
    genre.pendingGenre = -1;
    turn(&genre, 0, 5);
    assert(genre.pendingGenre == 5); // sixth genre is reachable
    turn(&genre, 0, 1);
    assert(genre.pendingGenre == -1); // wraps back to House
}
