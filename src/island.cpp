#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include "semaphore.h"
#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include "semaphore.h"

struct Boat;
struct Person;

// Only two locations
enum Loc { ISLAND, MAINLAND };

struct Person {
    int id;              // printed id (1..A for adults, 1..C for children)
    bool isAdult;
    Boat* boat;
    Loc position = ISLAND;   // where this person currently is
    int consectiveRows = 0;  // how many times in a row they’ve driven
    std::thread th;

    void run();
    void adultLoop();
    void childLoop();
};

struct Boat {
    // current occupants
    Person* driver = nullptr;
    Person* passenger = nullptr;

    // counts on the island
    int adultsOnIsland;
    int childrenOnIsland;
    Loc location = ISLAND;

    // semaphores
    Semaphore mutex{1};         // protects all shared state
    Semaphore seatSlots{2};     // capacity of 2 seats
    Semaphore needDriver{0};    // driver exists
    Semaphore readyToDepart{0}; // composition valid; boat may depart
    Semaphore tripDone{0};      // each rider waits once per trip

    // phase for shuttle policy
    enum Phase { KIDKID_OUT, KID_BACK, ADULT_WITH_KID, KID_BACK2, KIDS_FINISH };
    Phase phase = KIDKID_OUT;

    // stats for summary
    int tripsToMain = 0, tripsToIsland = 0;
    int twokidBoats = 0, kidAdultBoats = 0, soloBoats = 0;
    int adultDrivers = 0, childDrivers = 0;

    // RNG for 1–4 sec trip time
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist{1, 4};

    Boat(int A, int C) : adultsOnIsland(A), childrenOnIsland(C) {}

    // ---- policy helpers (called with mutex held) ----
    bool adultsRemain() const { return adultsOnIsland > 0; }
    bool kidsRemain()   const { return childrenOnIsland > 0; }

    bool canBoardAsPassenger(Person* p) {
        // never allow 2 adults in the boat
        if (driver && driver->isAdult && p->isAdult) return false;
        return true;
    }

    bool compositionValid() {
        // must have a driver
        if (!driver) return false;
        // solo driver trip is allowed
        if (!passenger) return true;
        // forbid 2 adults
        if (driver->isAdult && passenger->isAdult) return false;
        // 2 kids or kid+adult is fine
        return true;
    }

    bool phaseAllows(Person* p, bool wantsDriver) {
        switch (phase) {
            case KIDKID_OUT:
                // two kids out to mainland
                return !p->isAdult;

            case KID_BACK:
                // one kid returns to the island
                return !p->isAdult;

            case ADULT_WITH_KID:
                if (wantsDriver) {
                    // kid must drive
                    return !p->isAdult;
                } else {
                    // adult as passenger
                    return p->isAdult;
                }

            case KID_BACK2:
                // one kid returns again to reset for next cycle
                return !p->isAdult;

            case KIDS_FINISH:
                // Final trip(s): only children, and boat must start on island.
                if (p->isAdult) return false;
                return (location == ISLAND);
        }
        return true; // defensive
    }


    void nextPhase_unlocked() {
        // Complete the 4-step shuttle pattern first. Only after the kid
        // has rowed back in KID_BACK2 and there are no adults left do we
        // switch to the final "kids only" phase.
        if (phase == KIDKID_OUT) {
            phase = KID_BACK;
        } else if (phase == KID_BACK) {
            phase = ADULT_WITH_KID;
        } else if (phase == ADULT_WITH_KID) {
            phase = KID_BACK2;
        } else if (phase == KID_BACK2) {
            if (!adultsRemain())
                phase = KIDS_FINISH;   // now safe to finish with kids only
            else
                phase = KIDKID_OUT;    // start another adult cycle
        }
    }

    // ---- boarding ----
    bool tryBoard(Person* p, bool wantsDriver) {
        // must be on the same side as the boat
        mutex.wait();
        bool sameSide = (location == p->position);
        mutex.signal();
        if (!sameSide) return false;

        // Pre-check under mutex (may flip to passenger if driver already present)
        mutex.wait();
        if (wantsDriver && driver != nullptr &&
            passenger == nullptr && canBoardAsPassenger(p)) {
            wantsDriver = false; // someone already driving → take passenger seat
        }

        bool preOk = (location == p->position) &&
                     phaseAllows(p, wantsDriver) &&
                     (wantsDriver ? (driver == nullptr) : (passenger == nullptr)) &&
                     (wantsDriver ? (p->consectiveRows < 4) : true) &&
                     canBoardAsPassenger(p);
        mutex.signal();

        if (!preOk) return false;

        // wait for a seat
        seatSlots.wait();

        // Re-check after possibly waiting (state may have changed)
        mutex.wait();
        if (wantsDriver && driver != nullptr &&
            passenger == nullptr && canBoardAsPassenger(p)) {
            wantsDriver = false;
        }

        bool ok = (location == p->position) &&
                  phaseAllows(p, wantsDriver) &&
                  (wantsDriver ? (driver == nullptr) : (passenger == nullptr)) &&
                  (wantsDriver ? (p->consectiveRows < 4) : true) &&
                  canBoardAsPassenger(p);

        if (!ok) {
            mutex.signal();
            seatSlots.signal();
            return false;
        }

        if (wantsDriver) {
            driver = p;
            std::cout << (p->isAdult ? "Adult " : "Child ") << p->id
                      << " got into the driver's seat of the boat." << std::endl;
            needDriver.signal();
        } else {
            passenger = p;
            std::cout << (p->isAdult ? "Adult " : "Child ") << p->id
                      << " got into the passenger seat of the boat." << std::endl;
        }

        if (compositionValid()) {
            // we now have a valid boat (solo or pair) → allow departure
            readyToDepart.signal();
        }

        mutex.signal();
        return true;
    }

    // ---- trip ----
    void departIfDriver(Person* me) {
        // Only the driver should proceed here
        needDriver.wait();    // ensure someone declared as driver
        readyToDepart.wait(); // wait until composition is valid

        // Lock and pre-flip location so no one can “board mid-trip”
        Loc start, dest;
        mutex.wait();
        start = location;
        dest  = (location == ISLAND ? MAINLAND : ISLAND);
        location = dest;      // mark boat as already at destination for boarding logic
        mutex.signal();

        std::cout << "Boat is traveling from "
                  << (start == ISLAND ? "island"   : "mainland")
                  << " to "
                  << (dest  == ISLAND ? "island"   : "mainland")
                  << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(dist(rng)));

        // Update shared state
        mutex.wait();

        // move each rider and update island counts
        auto movePerson = [&](Person* x) {
            if (!x) return;
            if (start == ISLAND) {
                // going island -> mainland
                if (x->isAdult) adultsOnIsland--;
                else            childrenOnIsland--;
                x->position = MAINLAND;
            } else {
                // going mainland -> island
                if (x->isAdult) adultsOnIsland++;
                else            childrenOnIsland++;
                x->position = ISLAND;
            }
        };

        movePerson(driver);
        movePerson(passenger);

        if (start == ISLAND) tripsToMain++;
        else                 tripsToIsland++;

        // stats about boat contents
        if (driver && passenger) {
            if (!driver->isAdult && !passenger->isAdult) twokidBoats++;
            else                                         kidAdultBoats++;
        } else {
            soloBoats++;
        }
        if (driver) {
            if (driver->isAdult) adultDrivers++;
            else                 childDrivers++;
        }

        // row quota tracking
        if (driver)    driver->consectiveRows++;
        if (passenger) passenger->consectiveRows = 0; // passenger is a break

        // how many riders were actually on board?
        int riders = 0;
        if (driver)    riders++;
        if (passenger) riders++;

        // clear boat & advance phase
        driver = passenger = nullptr;
        nextPhase_unlocked();

        mutex.signal();

        // free the seats we actually used
        for (int i = 0; i < riders; ++i) seatSlots.signal();
        // signal end-of-trip to each rider (driver + passenger)
        for (int i = 0; i < riders; ++i) tripDone.signal();
    }

    bool allDone() {
        mutex.wait();
        // done when no one is on the ISLAND anymore
        bool done = (adultsOnIsland == 0 && childrenOnIsland == 0);
        mutex.signal();
        return done;
    }

    void summary() {
        std::cout << "Summary of Events" << std::endl;
        std::cout << "Boat traveled to the mainland: " << tripsToMain << std::endl;
        std::cout << "Boat returned to the island: "   << tripsToIsland << std::endl;
        std::cout << "Boats with 2 children: "         << twokidBoats << std::endl;
        std::cout << "Boats with 1 child and 1 adult: " << kidAdultBoats << std::endl;
        std::cout << "Boats with only 1 person (child or adult): "
                  << soloBoats << std::endl;
        std::cout << "Times adults where the driver: "   << adultDrivers << std::endl;
        std::cout << "Times children where the driver: " << childDrivers << std::endl;
    }
};

static Boat* gBoat = nullptr;

// -------- Person methods --------
void Person::run() {
    if (isAdult) adultLoop();
    else         childLoop();
}

void Person::adultLoop() {
    // Adults: only go ISLAND -> MAINLAND, once, as passenger with child driver
    while (true) {
        if (gBoat->allDone()) return;
        if (!gBoat->tryBoard(this, /*wantsDriver=*/false)) continue;
        // adult is aboard as passenger; wait for trip to finish
        gBoat->tripDone.wait();
        return; // done after reaching mainland
    }
}

void Person::childLoop() {
    while (true) {
        if (gBoat->allDone()) return;

        // children prefer to drive unless they’ve hit 4 consecutive rows
        bool wantsDriver = (consectiveRows < 4);

        if (!gBoat->tryBoard(this, wantsDriver)) continue;

        // after boarding, check if we actually became the driver
        bool amDriver;
        gBoat->mutex.wait();
        amDriver = (gBoat->driver == this);
        gBoat->mutex.signal();

        if (amDriver) {
            // I’m the driver
            gBoat->departIfDriver(this);
            gBoat->tripDone.wait();  // wait for my own trip completion signal
        } else {
            // I’m passenger
            gBoat->tripDone.wait();
            consectiveRows = 0;      // being passenger is a break
        }
    }
}

// -------- main --------
int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: ./bin/island <adults> <children>" << std::endl;
        return 1;
    }
    int A = std::stoi(argv[1]);
    int C = std::stoi(argv[2]);
    if (A <= 0 || C <= 0) {
        std::cerr << "inputs must be > 0" << std::endl;
        return 1;
    }

    Boat boat(A, C);
    gBoat = &boat;

    std::vector<std::unique_ptr<Person>> people;
    people.reserve(A + C);

    // Adults: IDs 1..A
    for (int i = 0; i < A; ++i) {
        auto p = std::make_unique<Person>();
        p->id = i + 1;            // Adult IDs 1..A
        p->isAdult = true;
        p->boat = gBoat;
        p->position = ISLAND;
        people.push_back(std::move(p));
    }
    // Children: IDs 1..C
    for (int i = 0; i < C; ++i) {
        auto p = std::make_unique<Person>();
        p->id = i + 1;            // Child IDs 1..C
        p->isAdult = false;
        p->boat = gBoat;
        p->position = ISLAND;
        people.push_back(std::move(p));
    }

    // start all threads
    for (auto& p : people) {
        p->th = std::thread(&Person::run, p.get());
    }
    // join all threads
    for (auto& p : people) {
        p->th.join();
    }

    boat.summary();
    return 0;
}
