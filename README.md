# Your Name and Drexel ID
Name: Samii Shabuse
Email: sus24@drexel.edu

# Instructions

## How to Run The Code
```bash
make
./bin/island <adults> <children>

# or you can also do

make run # runs 7 adults, 9 children
```
## IMPORTANT NOTE

My approach assumes there are enough children available to safely shuttle the boat.
Specifically:

- There must be at least 2 children total (C >= 2).
- The number of children must be at least adults + 1 (C >= A + 1).

If these conditions are not met, my program prints a clear error and exits instead
of getting stuck. I added this validation in parse_args because I was not able
to design a safe algorithm that handles those configurations under the problem’s
constraints.

My approach will not work if there are more adults than children, and if there aren't atleast 2 children on the boat. I wasn't able come up with an approach that is able to handle that. I adjusted the input validation to prevent that style of input.

# Short Essay Question 1: What did you use to protect the boat and why?

I used std::mutex for the Boat to protect all shared boat state and std::condition_variable for (`Person::cv` and `Boat::tripDoneCv`) to coordinate threads. The mutex ensures updates to fields like driver, passenger, boardedCount, and the island counts are serialized and free of data races. Condition variables are used to wake specific person threads when they are assigned roles and to let the controller wait for trip completion. The code intentionally releases the lock during the simulated travel so other threads can make progress while the boat is in transit.

# Short Essay Question 2: How did threads decide what position to take in the boat?

The main thread (controller) decides positions by selecting suitable people with the find_person helper and assigning boat.driver and boat.passenger pointers. After assigning roles the controller sets each person's role to either DRIVER or PASSENGER and calls that person's cv.notify_one() function so the person thread wakes and acts. Each Person::run() waits on its cv, then checks its role and performs driver or passenger actions. Selection respects constraints like location, consecutiveRows, and needsBreak, and prefers people under the consecutive-row limit while having a fallback to avoid blocking progress based on the instructions requirements of only max of 
4 times.

# Short Essay Question 3: How did you reset the boat for the next group?

After a trip finishes the driver thread updates rider positions and island counts, adjusts trip statistics, and updates per-person fields under the boat mutex. It then clears boat.driver and boat.passenger, by setting  them to nullptr, resets boardedCount to zero, and sets each participant's role back to NONE and seated to false. The passenger's consecutiveRows is reset and the driver's counter is incremented, and possibly setting needsBreak depending on number of times, ensuring the MAX_CONSECUTIVE policy is enforced. Finally the driver calls boat.tripDoneCv.notify_all() to wake the controller and any waiting passengers so the controller can choose the next group.

# Short Essay Question 4: Why are you certain everyone will get off the island?

The controller runs a deterministic algorithm that guarantees progress. This is done with each main cycle moving one adult from island to mainland, either two children shuttling and an adult+child trip, monotonically decreasing adultsOnIsland variable. After all adults are moved the controller ferries remaining children in pairs, or solo if odd, so the number of people on the island always decreases. The find_person contains a fallback that relaxes the consecutive-row preference to avoid permanent blocking from needsBreak, and the controller always notifies selected threads and waits correctly for trip completion. Each Person::run() checks the global termination condition and exits when they are on the mainland and nobody remains on the island, so everyone eventually leaves guaranteeing there will always be people coming off the island.

# Short Essay Question 5: What was the most challenging part of this assignment?

The hardest part was correctly coordinating multiple threads with locking and signaling so there are no races or deadlocks while preserving the alogrithm to work. That includes deciding which state to protect with the boat mutex, using per-person condition variables for targeted wakeups, and ensuring the driver waits for the passenger to be seated without busy-waiting. Implementing the MAX_CONSECUTIVE constraint and needsBreak logic so drivers don't starve others while still guaranteeing global progress was also tricky, because it got in the way and forced me to change my original approach. Finally, releasing the mutex during simulated travel while safely updating shared state before and after that sleep required ordering which took some time.