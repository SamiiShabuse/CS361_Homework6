/**
 * @file src/island.cpp
 * 
 * @brief Simulation of ferrying adults and children between an island and mainland using threads.
 * 
 * @author Samii Shabuse <sus24@drexel.edu>
 * @date November 16, 2025
 * 
 * @section Overview
 * 
 * This program simulates the problem of ferrying adults and children between an island and mainland
 * using a boat that can carry either one adult or up to two children at a time. The program uses threads
 * to represent each person (adult or child) and employs synchronization mechanisms to ensure safe
 * and orderly boat trips. The boat trips are orchestrated by a controller loop that follows a
 * deterministic strategy to minimize the number of trips while adhering to the constraints of the boat's capacity
 * and the maximum consecutive rowing limit for each person.
 */

#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <memory>

// Global boat state of either ISLAND or MAINLAND
enum Loc { ISLAND, MAINLAND };

// maximum consecutive times a person may drive the boat
static const int MAX_CONSECUTIVE = 4; 

// forward declaration of Boat structure
struct Boat;

/**
 * @struct Person
 * 
 * @brief Represents a person (adult or child) trying to cross between island and mainland.
 * 
 * The struct contains the person's ID, type (adult/child), current position,
 * consecutive rowing count, role in the boat (driver/passenger/none), and
 * threading constructs for synchronization.
 */
struct Person {
    int id;
    bool isAdult;
    Loc position = ISLAND;
    int consecutiveRows = 0; // how many times they've rowed in a row

    // assignment state (protected by boat->mtx)
    enum Role { NONE, DRIVER, PASSENGER } role = NONE;
    bool seated = false;
    bool needsBreak = false; // true when reached MAX_CONSECUTIVE and needs a break

    std::condition_variable cv;
    std::thread th;

    Boat* boat = nullptr;

    void run();
};

/**
 * @struct Boat
 * 
 * @brief Represents the state of the boat and manages synchronization between persons.
 * 
 * The struct contains mutexes and condition variables for thread synchronization,
 * the current location of the boat, counts of adults and children on the island,
 * pointers to the current driver and passenger, and various statistics.
 */
struct Boat {
    std::mutex mtx;
    std::condition_variable tripDoneCv; // controller waits for trip completion

    Loc location = ISLAND;

    int adultsOnIsland = 0;
    int childrenOnIsland = 0;

    Person* driver = nullptr;
    Person* passenger = nullptr;
    int boardedCount = 0;

    // stats
    int tripsToMain = 0, tripsToIsland = 0;
    int twokidBoats = 0, kidAdultBoats = 0, soloBoats = 0;
    int adultDrivers = 0, childDrivers = 0;

    // RNG
    std::mt19937 rng{std::random_device{}()};
    // RNG for 1–4 second trip time said by the homework requirements
    std::uniform_int_distribution<int> dist{1,4};

    /**
     * @brief Generates a random trip time between 1 and 4 seconds.
     * 
     * @param void
     * 
     * @return int Random trip time in seconds.
     * 
     * @details Uses the boat's internal uniform distribution and RNG to produce a value in the inclusive range [1,4].
     */
    int tripTime() { return dist(rng); }
};

static Boat* gBoat = nullptr;

/**
 * @brief The main execution loop for each Person thread.
 * 
 * @param void
 * 
 * @return void
 * 
 * @details person waits for their assignment as a driver or passenger,
 * performs the boat trip, updates the boat and personal state,
 * and handles termination conditions.
 */
void Person::run() {
    std::unique_lock<std::mutex> lk(gBoat->mtx);
    while (true) {
        // exit condition: I'm on mainland and won't be needed anymore
        if (gBoat->adultsOnIsland == 0 && gBoat->childrenOnIsland == 0 && position == MAINLAND) {
            return;
        }

        // Wait for assignment or final termination (when everyone is on mainland)
        cv.wait(lk, [&]{
            return role != NONE || (gBoat->adultsOnIsland == 0 && gBoat->childrenOnIsland == 0 && position == MAINLAND);
        });

        // if assigned as driver
        if (role == DRIVER) {
            // print boarding
            std::cout << (isAdult ? "Adult " : "Child ") << id
                      << " got into the driver's seat of the boat." << std::endl;
            seated = true;
            gBoat->boardedCount++;

            // wait until passenger (if any) is seated as well
            while (gBoat->passenger != nullptr && !gBoat->passenger->seated) {
                // release lock briefly to let passenger proceed
                gBoat->mtx.unlock();
                std::this_thread::yield();
                gBoat->mtx.lock();
            }

            // start trip: perform travel (release lock during sleep)
            Loc start = gBoat->location;
            Loc dest = (start == ISLAND ? MAINLAND : ISLAND);
            gBoat->location = dest; // preflip

            std::cout << "Boat is traveling from "
                      << (start==ISLAND?"island":"mainland")
                      << " to " << (dest==ISLAND?"island":"mainland") << std::endl;

            int t = gBoat->tripTime();
            gBoat->mtx.unlock();
            std::this_thread::sleep_for(std::chrono::seconds(t));
            gBoat->mtx.lock();

            // move riders and update counts
            auto movePerson = [&](Person* p) {
                if (!p) return;
                if (start == ISLAND) {
                    if (p->isAdult) gBoat->adultsOnIsland--;
                    else gBoat->childrenOnIsland--;
                    p->position = MAINLAND;
                } else {
                    if (p->isAdult) gBoat->adultsOnIsland++;
                    else gBoat->childrenOnIsland++;
                    p->position = ISLAND;
                }
            };

            movePerson(this);
            movePerson(gBoat->passenger);

            if (start == ISLAND) gBoat->tripsToMain++; else gBoat->tripsToIsland++;

            // stats
            if (gBoat->driver && gBoat->passenger) {
                if (!gBoat->driver->isAdult && !gBoat->passenger->isAdult) gBoat->twokidBoats++;
                else gBoat->kidAdultBoats++;
            } else {
                gBoat->soloBoats++;
            }
            if (gBoat->driver) {
                if (gBoat->driver->isAdult) gBoat->adultDrivers++; else gBoat->childDrivers++;
            }

            // consecutive row handling
            if (gBoat->driver) {
                gBoat->driver->consecutiveRows++;
                if (gBoat->driver->consecutiveRows >= MAX_CONSECUTIVE) gBoat->driver->needsBreak = true;
            }
            if (gBoat->passenger) {
                gBoat->passenger->consecutiveRows = 0;
                gBoat->passenger->needsBreak = false;
            }

            // clear boat pointers and boarded flags
            gBoat->driver = nullptr;
            gBoat->passenger = nullptr;
            gBoat->boardedCount = 0;

            // // debug status
            // std::cout << "[Status] location=" << (gBoat->location==ISLAND?"island":"mainland")
            //           << ", adultsOnIsland=" << gBoat->adultsOnIsland
            //           << ", childrenOnIsland=" << gBoat->childrenOnIsland << std::endl;

            // wake controller and any passenger waiting (trip done)
            gBoat->tripDoneCv.notify_all();

            // reset my role/seated
            role = NONE;
            seated = false;

            // if I'm on mainland now and nobody needs me, I may exit in next loop
            continue;
        }

        // if assigned passenger
        if (role == PASSENGER) {
            std::cout << (isAdult ? "Adult " : "Child ") << id
                      << " got into the passenger seat of the boat." << std::endl;
            seated = true;
            gBoat->boardedCount++;

            // wait for trip completion
            gBoat->tripDoneCv.wait(lk);

            // after trip, reset role/seated
            role = NONE;
            seated = false;
            continue;
        }
    }
}

/**
 * @brief Parse program arguments for number of adults and children.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param A Output adults count (filled on success).
 * @param C Output children count (filled on success).
 * 
 * @return true if parsing succeeded, false otherwise.
 * 
 * @details Validates that exactly two numeric arguments are provided and
 *          that both numbers are greater than zero. On success `A` and `C`
 *          are set to the parsed values.
 */
bool parse_args(int argc, char** argv, int &A, int &C) {
    if (argc != 3) {
        std::cerr << "usage: ./bin/island <adults> <children>" << std::endl;
        return false;
    }

    try {
        A = std::stoi(argv[1]);
        C = std::stoi(argv[2]);
    } catch (...) {
        std::cerr << "inputs must be integers" << std::endl;
        return false;
    }

    if (A <= 0 || C <= 0) {
        std::cerr << "inputs must be > 0" << std::endl;
        return false;
    }

    if (C < 2) {
        std::cerr << "Error: At least two children are required to operate the boat." << std::endl;
        return false;
    }

    if (C < A + 1) {
        std::cerr << "Error: Impossible to evacuate all adults with only "
                  << C << " children and " << A << " adults." << std::endl;
        return false;
    }

    return true;
}


/**
 * @brief Initialize person objects and assign them to the boat.
 *
 * @param boat Pointer to the shared Boat instance.
 * @param A Number of adults.
 * @param C Number of children.
 * 
 * @return std::vector<std::unique_ptr<Person>> Container of created people.
 * 
 * @details Allocates `A` adult and `C` child `Person` instances, sets their
 *          initial positions to `ISLAND`, assigns the shared `boat` pointer,
 *          and returns the owning vector of unique pointers.
 */
std::vector<std::unique_ptr<Person>> init_people(Boat* boat, int A, int C) {
    std::vector<std::unique_ptr<Person>> people;
    people.reserve(A + C);
    for (int i = 0; i < A; ++i) {
        auto p = std::make_unique<Person>();
        p->id = i+1;
        p->isAdult = true;
        p->position = ISLAND;
        p->boat = boat;
        people.push_back(std::move(p));
    }
    for (int i = 0; i < C; ++i) {
        auto p = std::make_unique<Person>();
        p->id = i+1;
        p->isAdult = false;
        p->position = ISLAND;
        p->boat = boat;
        people.push_back(std::move(p));
    }
    return people;
}

/**
 * @brief Find a person matching criteria.
 *
 * @param people Container of person pointers.
 * @param wantAdult true for adult, false for child.
 * @param where Location to search (ISLAND/MAINLAND).
 * @param excludeNeedsBreak whether to exclude those needing a break.
 * 
 * @return Person* or nullptr if none found.
 * 
 * @details Scans `people` for a person matching the requested age and
 *          location, who is not already assigned (`role == NONE`). The
 *          first pass prefers persons below the consecutive-row limit; the
 *          second pass relaxes that preference but still honors
 *          `excludeNeedsBreak` if requested.
 */
Person* find_person(std::vector<std::unique_ptr<Person>> &people, bool wantAdult, Loc where, bool excludeNeedsBreak = true) {
    for (auto &p : people) {
        if (p->isAdult != wantAdult) continue;
        if (p->position != where) continue;
        if (p->role != Person::NONE) continue;
        if (excludeNeedsBreak && p->needsBreak) continue;
        if (p->consecutiveRows >= MAX_CONSECUTIVE) continue;
        return p.get();
    }
    for (auto &p : people) {
        if (p->isAdult != wantAdult) continue;
        if (p->position != where) continue;
        if (p->role != Person::NONE) continue;
        if (excludeNeedsBreak && p->needsBreak) continue;
        return p.get();
    }
    return nullptr;
}

/**
 * @brief Controller loop that orchestrates deterministic ferrying of people.
 *
 * @param boat Reference to shared Boat.
 * @param people Container of people.
 * 
 * @return void
 * 
 * @details Implementation: it repeatedly moves two children,
 *          returns one, ships an adult with a child driving, and returns a
 *          child, until all adults are moved; then it moves remaining
 *          children. The function holds the boat mutex while deciding and
 *          notifying riders, and releases it while waiting for trip
 *          completion.
 */
void controller_loop(Boat &boat, std::vector<std::unique_ptr<Person>> &people) {
    std::unique_lock<std::mutex> lk(boat.mtx);

    while (boat.adultsOnIsland > 0) {
        // 1) Two children go island -> mainland
        Person* c1 = find_person(people, false, ISLAND, /*excludeNeedsBreak=*/false);
        Person* c2 = nullptr;
        if (c1) {
            for (auto &p : people) if (!p->isAdult && p->position==ISLAND && p->role==Person::NONE && p.get()!=c1) { c2 = p.get(); break; }
        }
        if (!c1 || !c2) break;
        boat.driver = c1; boat.passenger = c2;
        c1->role = Person::DRIVER; c2->role = Person::PASSENGER; c1->seated = c2->seated = false;
        c1->cv.notify_one(); c2->cv.notify_one();
        boat.tripDoneCv.wait(lk);

        // 2) One child returns mainland -> island
        Person* rc = find_person(people, false, MAINLAND, /*excludeNeedsBreak=*/false);
        if (!rc) rc = find_person(people, true, MAINLAND, /*excludeNeedsBreak=*/false);
        if (!rc) break;
        boat.driver = rc; boat.passenger = nullptr; rc->role = Person::DRIVER; rc->seated = false; rc->cv.notify_one();
        boat.tripDoneCv.wait(lk);

        // 3) One adult + one child go island -> mainland (child drives)
        Person* adult = find_person(people, true, ISLAND, false);
        Person* child = find_person(people, false, ISLAND, false);
        if (!adult || !child) break;
        boat.driver = child; boat.passenger = adult; child->role = Person::DRIVER; adult->role = Person::PASSENGER;
        child->seated = adult->seated = false; child->cv.notify_one(); adult->cv.notify_one();
        boat.tripDoneCv.wait(lk);

        // 4) One child returns mainland -> island
        Person* rc2 = find_person(people, false, MAINLAND, false);
        if (!rc2) rc2 = find_person(people, true, MAINLAND, false);
        if (!rc2) break;
        boat.driver = rc2; boat.passenger = nullptr; rc2->role = Person::DRIVER; rc2->seated = false; rc2->cv.notify_one();
        boat.tripDoneCv.wait(lk);
    }

    // Move remaining children in pairs (or solo)
    while (boat.childrenOnIsland > 0) {
        if (boat.childrenOnIsland >= 2) {
            Person* c1 = find_person(people, false, ISLAND);
            Person* c2 = nullptr;
            if (c1) for (auto &p : people) if (!p->isAdult && p->position==ISLAND && p->role==Person::NONE && p.get()!=c1) { c2 = p.get(); break; }
            if (c1 && c2) {
                boat.driver = c1; boat.passenger = c2;
                c1->role = Person::DRIVER; c2->role = Person::PASSENGER;
                c1->seated = c2->seated = false;
                c1->cv.notify_one(); c2->cv.notify_one();
                boat.tripDoneCv.wait(lk);
            } else break;
        } else {
            Person* c = find_person(people, false, ISLAND);
            if (c) {
                boat.driver = c; c->role = Person::DRIVER; c->seated=false; c->cv.notify_one();
                boat.tripDoneCv.wait(lk);
            } else break;
        }
    }

    // wake anyone still blocked to let threads exit
    for (auto &p : people) p->cv.notify_one();

    // unlock while joining threads
    lk.unlock();
}

/**
 * @brief Start threads for all people in container.
 *
 * @param people Container of people whose threads will be started.
 * 
 * @return void
 * 
 * @details Creates a `std::thread` for each `Person` that runs
 *          `Person::run()` and stores it in the person's `th` member.
 */
void start_threads(std::vector<std::unique_ptr<Person>> &people) {
    for (auto &p : people) p->th = std::thread(&Person::run, p.get());
}

/**
 * @brief Join all threads in people container.
 *
 * @param people Container of people whose threads will be joined.
 * 
 * @return void
 * 
 * @details Joins each person's thread if it is joinable to ensure clean
 *          termination before the program exits.
 */
void join_threads(std::vector<std::unique_ptr<Person>> &people) {
    for (auto &p : people) if (p->th.joinable()) p->th.join();
}

/**
 * @brief Print a concise summary of the boat statistics.
 *
 * @param boat Reference to the Boat whose stats will be printed.
 * 
 * @return void
 * 
 * @details Prints trip counts and driver statistics collected during the
 *          simulation to standard output.
 */
void print_summary(Boat &boat) {
    std::cout << "Summary of Events" << std::endl;
    std::cout << "Boat traveled to the mainland: " << boat.tripsToMain << std::endl;
    std::cout << "Boat returned to the island: " << boat.tripsToIsland << std::endl;
    std::cout << "Boats with 2 children: " << boat.twokidBoats << std::endl;
    std::cout << "Boats with 1 child and 1 adult: " << boat.kidAdultBoats << std::endl;
    std::cout << "Boats with only 1 person (child or adult): " << boat.soloBoats << std::endl;
    std::cout << "Times adults where the driver: " << boat.adultDrivers << std::endl;
    std::cout << "Times children where the driver: " << boat.childDrivers << std::endl;
}

/**
 * @brief Main function to initialize the boat and persons, start threads, and manage the simulation.
 * 
 * @param argc Argument count.
 * @param argv Argument vector containing number of adults and children.
 * 
 * @return int
 * 
 * @details Parses arguments, initializes state, starts person threads,
 *          runs the deterministic controller loop, joins threads, and
 *          prints a summary of the simulation.
 */
int main(int argc, char** argv) {

    int A = 0, C = 0;
    if (!parse_args(argc, argv, A, C)) return 1;

    Boat boat;
    boat.adultsOnIsland = A;
    boat.childrenOnIsland = C;
    gBoat = &boat;

    auto people = init_people(gBoat, A, C);
    start_threads(people);
    controller_loop(boat, people);
    join_threads(people);
    print_summary(boat);

    return 0;
}
