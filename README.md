# Table of Contents

- [Barrier](#barrier)
- [Resuable Barrier](#reusable-barrier)
- [Leaders and Followers (Two Queues)](#leaders-and-followers-two-queues)
- [Producers and Consumers](#producers-and-consumers)
    - [Infinite Buffer](#infinite-buffer)
    - [Finite Buffer](#finite-buffer)
- [Readers and Writers](#readers-and-writers)

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

