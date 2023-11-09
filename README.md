# Table of Contents

- [Barrier](#barrier)
- [Resuable Barrier](#reusable-barrier)
- [Leaders and Followers (Two Queues)](#leaders-and-followers-two-queues)
- [Producers and Consumers](#producers-and-consumers)
    - [Infinite Buffer](#infinite-buffer)
    - [Finite Buffer](#finite-buffer)
- [Readers and Writers](#readers-and-writers)
    - [Solution with starvation](#solution-with-starvation)
    - [No starvation solution](#no-starvation-solution)
    - [Writers prioritized](#solution-with-writers-prioritized)
- [No-starve Mutex](#no-starve-mutex)
- [Dining Philosophers](#dining-philosophers)
    - [Limiting active philosophers](#limiting-the-number-of-active-philosophers)
    - [Changing order of pick for one](#one-philosopher-picks-the-left-first)
    - [Tanenbaum's solution (starvation prone)](#tanenbaums-solution)
- [Cigarettes Smokers](#cigarette-smokers)
- [The Dining Savages](#the-dining-savages)
- [The Barbershop](#the-barbershop)
- [FIFO Barbershop](#fifo-barbershop)
- [Hilzer's Barbershop](#hilzers-barbershop)

## Barrier

We want n threads to rendezvous before entering a critical section i.e.,

```cpp
rendezvous          // all threads must come here before any of them entering the
                    // critical section
critical section
```

We use a counter for maintaining the number of threads that have rendezvoused, a semaphore for protecting access to this counter and a barrier semaphore which will be 
set by the last thread that completed rendezvous,

```cpp
counter = 0
barrier = semaphore(0)
mutex = semaphore(1)

// Each thread executes this code

mutex.wait()
counter = counter + 1
if (counter == n)
    barrier.signal()
mutex.signal()

/* Rendezvous here */

barrier.wait()
barrier.signal()

/* Critical Section */
```

## Reusable Barrier

We want to modify the barrier so that the threads synchronize before enterning the critical section and after leaving the critical section i.e., the barrier is run in a loop and before everyone is synchronized at the end of the critical section, no thread is allowed to go back to the start

To do this we use two barriers,

```cpp
counter = 0
barrier1 = semaphore(0)
barrier2 = semaphore(1)

// In each thread 

while (/* ... some loop condition */){
    mutex.wait()
    counter = counter + 1
    if (counter == n){
        barrier2.wait()             // lock barrier2 so none can pass through it
                                     // we can't have it 0 initially because we
                                     // we signal it one extra time in the end
        barrier1.signal()
    }
    mutex.signal()

    /* Rendezvous 1 */

    barrier1.wait()
    barrier1.signal()

    /* Critical Sectoin */

    mutex.wait()
    counter = counter - 1
    if (counter == 0){              // last thread to come at the end
                                    // all synchronized here so signal barrier2
                                    // and wait on barrier1 (since it has been signalled
                                    // an extra time)
        barrier1.wait()
        barrier2.signal()
    }
    mutex.signal()

    /* Rendezvous 2 */
    
    barrier2.wait()
    barrier2.signal()
}
```

## Leaders and Followers (Two Queues)

We've two queues, one on which leaders wait for followers and another on which followers wait for leaders. In the simple case where we just want to wait a follower/leader to arrive and then proceed the solution is pretty simple,

```cpp
leader_queue = semaphore(0)
follower_queue = semaphore(0)

// In follower thread
leader_queue.signal()
follower_queue.wait()
do_something()

// In leader thread
follower_queue.signal()
leader_queue.wait()
do_something()
```

But if we want a leader to a leader to invoke `do_something` concurrently with another follower or vice-versa, we've to do some more work,

```cpp
leaders = 0                         // #leaders waiting
followers = 0                       // #followers waiting
leader_queue = semaphore(0)
follower_queue = semaphore(0)
mutex = semaphore(0)                // to protect access to leaders & followers 
rendezvous = semaphore(0)           // to wait until both follower and leader 
                                    // finish executing do_something()
                                    // otherwise their could a situation 
                                    // where multiple leaders are executing
                                    // do_something concurrently 

// In follower thread
mutex.wait()
if (leaders > 0){                 
    // There is some leader waiting
    leaders = leaders - 1
    // signal it
    leader_queue.signal()
} else {
    // no leader waiting so wait on follower_queue
    followers = followers + 1
    mutex.signal()
    follower_queue.wait()
}

do_something()
rendezvous.wait()
mutex.signal()                      // this guarantees there's only one 
                                    // leader and follower executing do_something()
                                    // concurrently

// In leader thread
mutex.wait()
if (followers > 0){
    followers = followers - 1
    follower_queue.signal()
} else {
    leaders = leaders + 1
    mutex.signal()
    leader_queue.wait()
}

do_something()
rendezvous.signal()

// Note that only one thread (follower) release the mutex when a leader and follower
// pair is found and we know one of them holds the mutex
```

## Producers and Consumers

### Infinite buffer

```cpp
buffer = /* some data structure */
items = semaphore(0)    // for keeping track of available items in the buffer
mutex = semaphore(1)    // to protect access to buffer

void producer(){
    while (true){
        mutex.wait()
        buffer.add()    // add to buffer
        mutex.signal()
        items.signal()  // signal items
    }
}

void consumer(){
    while (true){
        items.wait()    // wait for buffer to be non-empty
        mutex.wait()
        buffer.remove()
        mutex.signal()
    }
}
```

### Finite buffer

Solution with mutexes (binary semaphores)

```cpp
buffer = 0
mutex = semaphore(0)
producer = semaphore(1)
consumer = semaphore(0)

void producer(){
    while (true){
        producer.wait()
        mutex.wait()
        // If buffer is full we don't signal
        // on producer
        if (buffer != MAX_BUFFER_SIZE){
            buffer = buffer + 1
            if (buffer == 1)        // first item in the buffer
                consumer.signal()
            producer.signal()
        }
        mutex.signal()
    }
}

void consumer(){
    while (true){
        consumer.wait()
        mutex.wait()
        // Similar to producer we don't signal
        // consumer if buffer is empty
        if (buffer){
            buffer = buffer - 1
            // signal producer now that buffer is not full
            if (buffer == MAX_BUFFER_SIZE - 1)
                producer.signal()
            consumer.signal()
        }
        mutex.signal()
    }
}
```

Solution with semaphores

```cpp
buffer = /* some data structure */
items = semaphore(0)                // the number of items currently in the buffer
can_add = semaphore(MAX_BUFFER_SIZE)    // the number of items we can add until buffer is full
mutex = semaphore(1)

void producer(){
    while (true){
        // block if we can't add any more items
        can_add.wait()

        mutex.wait()
        buffer.add()
        mutex.signal()

        items.signal()
    }
}

void consumer(){
    while (true){
        // block if buffer is empty
        items.wait()
        
        mutex.wait()
        buffer.remove()
        mutex.signal()

        can_add.signal()
    }
}
```

## Readers and Writers 

There are multiple readers and writers, multiple readers can be in the critical section simultaneously but a writer must have exclusive access to the entity,

### Solution with starvation

In this solution writers can be starved by the readers 

```cpp
accessible = semaphore(1)   // lock that must be held when accessing the critical section
mutex = semaphore(1)
readers = 0                 // number of readers in the critical section

void writer(){
    accessible.wait()    
    /* Critical Section */
    accessible.signal()
}

void reader(){
    mutex.wait()
    ++readers
    if (readers == 1)
        accessible.wait()   // first one in hold the lock
    mutex.signal()

    /* Critical Section */

    mutex.wait()
    --readers
    if (readers == 0)
        accessible.signal() // last one out releases the lock
    mutex.signal()
}
```

We use *Lightswitch* to denote the locking behavior of the readers like so 

```cpp
class Lightswitch{
    counter = 0
    mutex = semaphore(1)

    void lock(sem){
        mutex.wait()
        ++counter
        if (counter == 1)
            sem.wait()
        mutex.signal()
    }

    void unlock(sem){
        mutex.wait()
        --counter
        if (counter == 0)
            sem.signal()
        mutex.signal()
    }
};
```

We can then write our reader-writer solution in terms of it

```cpp
read_switch = Lightswitch{}
accessible = semaphore(1)

void writer(){
    accessible.wait()
    /* Critical Section */
    accessible.signal()
}

void reader(){
    read_switch.lock(accessible)
    /* Critical Section */
    read_switch.unlock(accessible)
}
```

### No starvation solution

To prevent starvation we can add another lock which a writer can hold when it's waiting, readers also need to hold this lock before proceeding, so if a writer wants to write all other readers/writers must block

```cpp
accessible = semaphore(1)
turnstile = semaphore(1)
read_switch = Lightswitch{}

void writer(){
    turnstile.wait()        // holding it will prevent any other reader/writer
                            // to proceed until this writer is done
    accessible.wait()

    /* Critical Section */

    turnstile.signal()
    accessible.signal()
}

void reader(){
    turnstile.wait()
    turnstile.signal()

    read_switch.lock(accessible)

    /* Critical Section */

    read_switch.unlock(accessible)
}
```

### Solution with writers prioritized 

This time we want the writers to be given more priority i.e., once a writer arrives no readers must be allowed to proceed until all the writers have left. We use logic similar to the read_switch in *reader()* above but this time in both readers and writers


```cpp
writer = semaphore(1)
writer_switch = Lightswitch{}
reader = semaphore(1)
reader_switch = Lightswitch{}

void writer(){
    writer_switch.lock(reader)  // block any more readers from entering until
                                // all writers have left
    writer.wait()

    /* Critical Section */

    writer.signal()
    writer_switch.unlock(reader)
}

void reader(){
    // readers must wait on reader before proceeding
    reader.wait()
    // then the readers lock writer to prevent
    // writers from entering critical section
    read_switch.lock(writer)
    reader.signal()

    /* Critical Section */

    read_switch.unlock(writer)
}
```

## No-starve Mutex

If a mutex is used in a loop, some threads can starve. We want to build a mutex which avoids this if the number of threads is finite. That is the number of threads which proceed before a waiting thread must be bounded

```cpp
room1 = 0
room2 = 0
mutex = semaphore(1)
t1 = semaphore(1)
t2 = semaphore(0)

// In threads

/* Locking phase */
mutex.wait()
++room1         // first we increment the number of waiting threads
mutex.signal()

t1.wait()
++room2         // threads which will be served in this pass
                // there can be atmost one thread here at this point
mutex.wait()
--room1
if (room1 == 0){    // all threads that need to be served are here so move to next phase
                    // since there're finite threads it must happen at some point
    mutex.signal()
    t2.signal()
} else {            // not all here unlock t1
    mutex.signal()
    t1.signal()
}

t2.wait()
--room2

/* Critical Section */

/* Unlocking phase */
if (room2 == 0)     // all threads that needed to be served are done so move to next pass
    t1.signal()
else                // allow other waiting threads to enter critical section
    t2.signal()
```

## Dining Philosophers

We've *n* philosphers sitting at a circular table, with *n* forks (one on either side of a philosopher). Each philosopher requires both the left and right forks to eat.   
We need to write a solution wherein philosophers are like threads and forks are exclusively available to only one at a time, there's no deadlock and starvation

### Limiting the number of active philosophers

We can prevent deadlock by limiting the number of philosophers which are allowed to eat at once to *n - 1*, thus breaking the cycle in the usual case

```cpp
allowed = semaphore(n - 1)
fork = array of mutexes for each fork

void get_forks(i){
    allowed.wait()
    fork[right[i]].wait()
    fork[left[i]].wait()
}

void put_forks(i){
    fork[right[i]].signal()
    fork[left[i]].signal()
    allowed.signal()
}
```

### One philosopher picks the left first

We can also prevent deadlock by changing the order in which one of the philosophers picks up their forks. There is no deadlock in this case because if there were it'd create a cycle in the mutex dependency which is impossible in this case

```cpp
void get_forks(i){
    if (i == 0){
        fork[left[i]].wait()
        fork[right[i]].wait()
    } else {
        fork[right[i]].wait()
        fork[left[i]].wait()
    }
}

void put_forks(i){
    if (i == 0){
        fork[left[i]].signal()
        fork[right[i]].signal()
    } else {
        fork[right[i]].signal()
        fork[left[i]].signal()
    }
}
```

### Tanenbaum's Solution

We maintain a vector of state the philosophers are in which can be 'THINKING', 'HUNGRY' and 'EATING' and a semaphore for each philosopher which tells if it can start eating or not.

**Note:** This approach is not starvation free. Suppose we have 5 philosophers and we want to starve 0

            0
          4   1
           3 2
Currently, 2 and 4 are eating. Now imagine the following events, 2 gets up, 1 starts eating (on 2's right), 4 gets up and 3 starts eating (on 4's right), now if 3 gets up, 4 starts eating (on 3's left) and 1 gets up and 2 starts eating, we're back where we started

```cpp
enum class state {
    THINKING, HUNGRY, EATING
}
curr_state = std::vector<state>(n, state::THINKING)
sem = std::vector<semaphore>(n)
mutex = semaphore(1)

void get_forks(i){
    mutex.wait()
    curr_state[i] = HUNGRY
    test(i)             // check if it can eat
    mutex.signal()
    sem[i].wait()
}

void put_forks(i){
    mutex.wait()
    curr_state[i] = THINKING
    test(right[i])
    test(left[i])
    mutex.signal()
}

void test(i){   // checks whether i can start eating
    if (curr_state[i] == 'HUNGRY' && curr_state[left[i]] != 'EATING' && 
    curr_state[right[i]] != 'EATING')
        curr_state[i] = 'EATING'
        sem[i].signal()
}
```

## Cigarette Smokers

In this problem we've an agent which generates two out of three ingredients : tobacco, paper or matches at a time. There are three smokers which loop repeatedly waiting for ingredients, making cigarettes and smoking. Each smoker requires only two out of the three ingredients and the sets are distinct.  

To solve this problem, we use 3 auxiliary threads called *pushers* henceforth

```cpp
tobacco_cnt = 0
paper_cnt = 0
match_cnt = 0
tobacco = semaphore(0)      // tobacco available?
paper = semaphore(0)
match = semaphore(0)
tabacco_sem = semaphore(0)  // can smoker with tobacco proceed
paper_sem = semaphore(0)
match_sem = semaphore(0)
mutex = semaphore(1)

void agent(){
    // Pick two ingredients randomly and signal respective sems
}

void smoker_with_tobacco(){
    while (true){
        tabacco_sem.wait()
        smoke()        
    }
}

void smoker_with_paper(){
    while (true){
        paper_sem.wait()
        smoke()
    }
}

void smoker_with_matches(){
    while (true){
        match_sem.wait()
        smoke()
    }
}

void pusher_tobacco(){
    tobacco.wait()      // wait for agent to supply tobacco

    mutex.wait()
    if (paper_cnt){     // we've tobacco and paper available
        --paper_cnt
        match_sem.signal()
    } else if (match_sem){
        --match_cnt
        paper_sem.signal()
    } else {
        ++tobacco_cnt
    }
    mutex.signal()
}

/* Similar code for pusher_paper and pusher_match */
```

## The Dining Savages

A number of savages consume from a pot that can hold a maximum of *m* servings, when a savage wants to eat he consumes from the pot if it isn't empty, otherwise he wakes up the cook and waits until the cook refills the pot (puts *m* servings in it)

```cpp
food = 0
mutex = semaphore(1)
empty = semaphore(0)        // is the pot empty?
full = semaphore(0)         // is the pot full?

void savage(){
    while (true){
        mutex.wait()
        if (food == 0){     // if no serving left signal the cook
            empty.signal()
            full.wait()     // wait until the cook finishes, this blocks all 
                            // other savages
        } 
        --food
        get_serving()
        mutex.signal()
        eat()
    }
}

void cook(){
    while (true){
        empty.wait()
        serve(m)
        food = m            // safe because a waiting savage holds the lock 
        full.signal()
    }
}
```

## The Barbershop

In this problem, there's a barber shop with a maximum of n seats. Customers come and sit on an empty sit if available otherwise they leave. The barber sleeps if there are no customers in the shop so when a customer arrives he'll wakeup the barber.

```cpp
n = 4
customers = 0                   // number of customers in the shop currently
// These four used for rendezvous
barber = semaphore(0)           // customer waits while barber wakes up
customer = semaphore(0)         // barber waits on a customer coming
barber_done = semaphore(0)      // barber done?
customer_done = semaphore(0)    // customer done

mutex = semaphore(1)

void customer(){
    mutex.wait()
    if (customers == n){
        mutex.signal()
        return
    }
    mutex.signal()

    // Rendezvous 1, wait for barber to wakeup
    customer.signal()
    barber.wait()

    /* Get haircut */

    // Rendezvous 2, wait for barber to finish
    customer_done.signal()
    barber_done.wait()

    // leave
    mutex.wait()
    --customers
    mutex.signal()
}

void barber(){
    while (true){
        customer.wait()
        barber.signal()

        /* Cut hair */

        barber_done.signal()
        customer_done.wait()
    }
}
```

## FIFO Barbershop

In this variation we've to serve the customers in the order they come. We can queue a to do this, where each thread creates its own semaphore and pushes it onto the queue and waits on it, the barber will signal on the semaphore at the front of the queue

```cpp
n = 4
queue = {}              
customer = semaphore(0)
barber_done = semaphore(0)
customer_done = semaphore(0)
mutex = semaphore(1)

void customer(){
    sem = semaphore(0)

    mutex.wait()
    if (customers == n){
        mutex.signal()
        return
    }
    ++customers
    queue.push(sem)
    mutex.signal()

    customer.signal()
    sem.wait()

    /* Get haircut */

    customer_done.signal()
    barber_done.wait()

    mutex.wait()
    --customers
    mutex.signal()
}

void barber(){
    while (true){
        customer.wait()
        mutex.wait()
        sem = q.front()
        q.pop()
        mutex.signal()

        sem.signal()

        /* Cut hair */

        barber_done.signal()
        customer_done.wait()
    }
}
```

## Hilzer's Barbershop

Our barbershop has 4 chairs, 4 barbers, and a waiting
area that can accommodate 4 customers on a sofa and that has
standing room for additional customers. Fire codes limit the total
number of customers in the shop to 20.  
A customer will not enter the shop if it is filled to capacity with
other customers. Once inside, the customer takes a seat on the sofa
or stands if the sofa is filled. When a barber is free, the customer
that has been on the sofa the longest is served and, if there are any
standing customers, the one that has been in the shop the longest
takes a seat on the sofa. When a customer’s haircut is finished,
any barber can accept payment, but because there is only one cash
register, payment is accepted for one customer at a time. The barbers divide their time among cutting hair, accepting payment, and
sleeping in their chair waiting for a customer.

In other words, the following synchronization constraints apply:  
- Customers invoke the following functions in order: enterShop, sitOnSofa,
getHairCut, pay.  
- Barbers invoke cutHair and acceptPayment.  
- Customers cannot invoke enterShop if the shop is at capacity.  
- If the sofa is full, an arriving customer cannot invoke sitOnSofa.  
- When a customer invokes getHairCut there should be a corresponding
barber executing cutHair concurrently, and vice versa.  
- It should be possible for up to three customers to execute getHairCut
concurrently, and up to three barbers to execute cutHair concurrently.  
- The customer has to pay before the barber can acceptPayment.  
- The barber must acceptPayment before the customer can exit.  

```cpp
capacity = 20                   // max shop capacity
sofa_queue = {}                 // customers on the sofa
waiting_queue = {}              // customers standing
customers = 0                   // total customers in the shop
customer = semaphore(0)         // customer available?
customer_done = semaphore(0)    
barber_done = semaphore(0)
count = semaphore(1)            // to protect access to customers and queues
register = semaphore(1)         // since only one can pay at a time
payed = semaphore(0)            // customer has payed
accepted = semaphore(0)         // payment has been accepted

void customer(){
    // semaphore on which the customer will wait for 
    // a barber to signal
    sem = semaphore(0)

    mutex.wait()
    if (capacity == 20){
        mutex.signal()
        return
    }
    ++capacity
    // sit on sofa if not full and no one else is standing
    if (sofa_queue.size() < 4 && waiting_queue.empty()){
        sofa_queue.push(sem)
        customer.signal()               // and signal barber
    } else {    // otherwise stand
        waiting_queue.push(sem) 
    } 
    mutex.signal()

    // wait for barber to be ready (either you signalled yourself
    // or someone leaving did it for you)
    sem.wait()

    /* Get haircut */

    customer_done.signal()
    barber_done.wait()

    // do payment
    register.wait()
    pay()
    payed.signal()
    accepted.wait()
    register.signal()

    mutex.wait()
    --capacity
    // if someone is standing give him seat on sofa
    if (!waiting_queue.empty()){
        sofa_queue.push(waiting_queue.front())
        waiting_queue.pop()
        customer.signal()       // and signal barber
    }
    mutex.signal()
}

void barber(){
    while (true){
        customer.wait()
        mutex.wait()
        sem = sofa_queue.front()
        sofa_queue.pop()
        mutex.signal()

        sem.signal()
        /* Cut hair */

        barber_done.signal();
        customer_done.wait();

        payed.wait()
        accept_payment()
        accepted_payment.signal()
    }
}
```

## Santa Claus 

Stand Claus sleeps in his shop at the North Pole and can only be
awakened by either  
- all nine reindeer being back from their vacation in the South Pacific, 
- or some of the elves having difficulty making toys;   

To allow Santa to get some sleep, the elves can only
wake him when three of them have problems. When three elves are
having their problems solved, any other elves wishing to visit Santa
must wait for those elves to return. If Santa wakes up to find three
elves waiting at his shop’s door, along with the last reindeer having
come back from the tropics, Santa has decided that the elves can
wait until after Christmas, because it is more important to get his
sleigh ready. (It is assumed that the reindeer do not want to leave
the tropics, and therefore they stay there until the last possible moment.) The last reindeer to arrive must get Santa while the others
wait in a warming hut before being harnessed to the sleigh.  

Here are some addition specifications:  
- After the ninth reindeer arrives, Santa must invoke prepareSleigh, and
then all nine reindeer must invoke getHitched.
- After the third elf arrives, Santa must invoke helpElves. Concurrently,
all three elves should invoke getHelp.
- All three elves must invoke getHelp before any additional elves enter
(increment the elf counter).  

Santa should run in a loop so he can help many sets of elves. We can assume
that there are exactly 9 reindeer, but there may be any number of elves.

```cpp

santa = semaphore(0)
mutex = semaphore(1)
reindeer = 0
elf = 0
elves = semaphore(1)     // mutex to restrict more than 3 elves from entering
sleigh = semaphore(0)    // to signal reindeers

void santa(){
    while (true){
        // wait for either 9 reindeers or 3 elves to signal
        santa.wait()
        
        mutex.wait()
        // prioritize reindeers
        if (reindeer >- 9){        
            prepareSleigh()
            sleigh.signal(9)
            reindeer -= 9
        } else {
            helpElves()
        }
        mutex.signal()
    }
}

void elf(){
    elves.wait()        
    mutex.wait()
    ++elf
    if (elf == 3)
        santa.signal()      // don't allow more than 3 elves to enter
    else 
        elves.signal()
    mutex.signal()

    getHelp()

    mutex.wait()
    --elf
    if (elf == 0)
        elves.signal()      // until all 3 have gotten help
    mutex.signal()
}

void reindeer(){
    mutex.wait()
    ++reindeer
    if (reindeer == 9)
        santa.signal()
    mutex.signal()

    sleigh.wait()
    getHitched()
}   
```

## Building H<sub>2</sub>O

There are two kinds of threads, oxygen and hydrogen. In order to assemble
these threads into water molecules, we have to create a barrier that makes each
thread wait until a complete molecule is ready to proceed.   
As each thread passes the barrier, it should invoke bond. You must guarantee
that all the threads from one molecule invoke bond before any of the threads
from the next molecule do.   
In other words:  
- If an oxygen thread arrives at the barrier when no hydrogen threads are
present, it has to wait for two hydrogen threads.
- If a hydrogen thread arrives at the barrier when no other threads are
present, it has to wait for an oxygen thread and another hydrogen thread.

## Solution with an extra conductor thread

```cpp
oxygen_queue = semaphore(0)
hydrogen_queue = semaphore(0)
oxygen = semaphore(0)
hydrogen = semaphore(0)
bonded = semaphore(0)
mutex = semaphore(1)
done = 0

void oxygen(){
    oxygen.signal()
    oxygen_queue.wait()
    bond()

    mutex.wait()
    ++done
    if (done == 3)
        bonded.signal()
    mutex.signal()
}

void hydrogen(){
    hydrogen.signal()
    hydrogen_queue.wait()

    mutex.wait()
    ++done
    if (done == 3)
        bonded.signal()
    mutex.signal()
}

void conductor(){
    while (true){
        oxygen.wait()
        hydrogen.wait()
        hydrogen.wait()

        // ready to make a molecule
        oxygen_queue.signal()
        hydrogen_queue.signal(2)

        bonded.wait()
        done = 0
    }
}
```

## Alternate solution

```cpp
oxygen = 0
hydrogen = 0
oxygen_queue = sempahore(0)
hydrogen_queue = sempahore(0)
mutex = semaphore(1)
barrier = semaphore(3)              // resets after hitting 0

void oxygen(){
    mutex.wait()
    ++oxygen
    if (hydrogen >= 2){             // we can form a molecule
        hydrogen -= 2
        hydrogen_queue.signal(2)
        --oxygen
        oxygen_queue.signal()   
        // we don't release mutex until all 3 bond
    } else {
        mutex.signal()
    }

    oxygen_queue.wait()
    bond()

    barrier.wait()
    mutex.signal()                  // either oxygen will hold the mutex or another
                                    // hydrogen which arrived last
}

void hydrogen(){
    mutex.wait()
    ++hydrogen
    if (hydrogen >= 2 && oxygen >= 1){
        hydrogen -= 2
        hydrogen_queue.signal(2)
        --oxygen
        oxygen_queue.signal()
    } else {
        mutex.signal()
    }

    hydrogen_queue.wait()

    bond()
    barrier.wait()
}
```

## River Crossing

Somewhere near Redmond, Washington there is a rowboat that is used by
both Linux hackers and Microsoft employees (serfs) to cross a river. The ferry
can hold exactly four people; it won’t leave the shore with more or fewer. To
guarantee the safety of the passengers, it is not permissible to put one hacker
in the boat with three serfs, or to put one serf with three hackers. Any other
combination is safe.  
As each thread boards the boat it should invoke a function called board. You
must guarantee that all four threads from each boatload invoke board before
any of the threads from the next boatload do.  
After all four threads have invoked board, exactly one of them should call
a function named rowBoat, indicating that that thread will take the oars. It
doesn’t matter which thread calls the function, as long as one does.

```cpp
hackers = 0
serfs = 0
mutex = semaphore(1)
hacker_queue = semaphore(0)
serf_queue = semaphore(0)
barrier = semaphore(4)

void hacker(){
    wiil_unlock = false     // will this thread row the boat?
    mutex.wait()
    ++hackers
    if (hackers >= 4){
        hackers -= 4
        hacker_queue.signal(4)
        wiil_unlock = true
    } else if (hackers >= 2 && serfs >= 2){
        hackers -= 2
        hacker_queue.signal(2)
        serfs -= 2
        serf_queue.signal(2)
        will_unlock = true
    } else {
        mutex.signal()
    }

    hacker_queue.wait()
    board()

    barrier.wait()
    if (will_unlock){       // the one rowing gets to unlock the mutex
        rowBoat()
        mutex.signal()
    }
}

void serf(){
    wiil_unlock = false
    mutex.wait()
    ++serfs
    if (serfs >= 4){
        serfs -= 4
        serf_queue.signal(4)
        wiil_unlock = true
    } else if (hackers >= 2 && serfs >= 2){
        hackers -= 2
        hacker_queue.signal(2)
        serfs -= 2
        serf_queue.signal(2)
        will_unlock = true
    } else {
        mutex.signal()
    }

    serf_queue.wait()
    board()

    barrier.wait()
    if (will_unlock){
        rowBoat()
        mutex.signal()
    }
}
```

## The Roller Coaster

Suppose there are n passenger threads and a car thread. The
passengers repeatedly wait to take rides in the car, which can hold
C passengers, where C < n. The car can go around the tracks only
when it is full.  
Here are some additional details:  
- Passengers should invoke board and unboard.
- The car should invoke load, run and unload.
- Passengers cannot board until the car has invoked load
- The car cannot depart until C passengers have boarded.
- Passengers cannot unboard until the car has invoked unload

```cpp
c = /* car capacity */
passengers = 0
load = semaphore(0)
car = semaphore(0)
mutex = semaphore(1)
unload = semaphore(0)
ready = semaphore(0)

void car(){
    while (true){
        load.signal(c)      // signal c customers
        car.wait()          // wait for everyone to board

        run()

        unload.signal(c)    // signal c customers to unboard
        ready.wait()        // wait for everyone to unboard
    }
}

void passenger(){       
    load.wait()
    mutex.wait()
    ++passengers
    if (passengers == c)
        car.signal()
    mutex.signal()

    unload.wait()           // wait for car to signal unloading

    mutex.wait()
    --passengers
    if (passengers == 0)
        ready.signal()
    mutex.signal()
}
```

## The Multi-Car Roller Coaster

This solution does not generalize to the case where there is more than one car.
In order to do that, we have to satisfy some additional constraints:  
- Only one car can be boarding at a time.
- Multiple cars can be on the track concurrently.
- Since cars can’t pass each other, they have to unload in the same order
they boarded.
- All the threads from one carload must disembark before any of the threads
from subsequent carloads.

```cpp
c = /* car capacity */
curr_car = 0
cars_mutex = semaphore(1)       // for protecting access to curr_car

boarded = 0
pass_mutex1 = semaphore(1)     
unboarded = 0
pass_mutex2 = semaphore(1)

boarding_queue = semaphore(0)
unboarding_queue = semaphore(0)
all_boarded = semaphore(0)
all_unboarded = semaphore(0)

car_unboard_order = {}      // list of m semaphores for each car for boarding and unboarding order
car_board_order = {}

// Initially car 0's sems are unlocked
car_unboard_order[0].signal()
car_board_order[0].signal()

void car(){
    car_id = 0
    cars_mutex.wait()           // assign id's in the order the cars board
        car_id = curr_car++
    cars_mutex.signal()

    car_board_order[car_id].wait()
    boarding_queue.signal(c)
    
    all_boarded.wait()          // wait for all passengers to board
    car_board_order[(car_id + 1) % m].signal()

    car_unboard_order[car_id].wait()                  // wait for unboarding turn
    unboarding_queue.signal(c)

    all_unboarded.wait()        // wait for everyone to unboard
    car_unboard_order[(car_id + 1) % m].signal()      // allow next car in unboard order to unboard
}

void passenger(){
    boarding_queue.wait()
    pass_mutex1.wait()
    ++boarded
    if (boarded == c){
        all_boarded.signal()
        boarded = 0
    }
    pass_mutex1.signal()

    unboarding_queue.wait()
    pass_mutex2.wait()
    ++unboarded
    if (unboarded == c){
        all_unboarded.signal()
        unboarded = 0
    }
    pass_mutex2.signal()
}
```