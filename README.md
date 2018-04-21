# Particle threads tutorial

[Threads](https://en.wikipedia.org/wiki/Thread_(computing)) allow concurrent execution of multiple bits of code. They're popular in desktop operating systems like Windows and in languages like Java. Threads have limited support in the Particle platform, but exist.

### Say no to threads

While this is a thread tutorial, in most cases you can get by without threads, and not using threads will make your life easier.

- Most APIs are not thread safe
- Limited memory and no virtual memory makes using threads impractical
- There is no way to stop a thread once started

If you look through the Windows or Java APIs, it's abundantly clear which API calls are thread-safe, because they are listed as MT-safe or not. The Particle APIs are generally not safe, but there's no single reference as to what is safe. 

Every thread must have its own stack, that's how threads work. The problem is that there is only about 80K of free memory on a Photon or Electron. The stack is normally 6K. Having more than a few threads will eat up your memory in no time! 

In Windows or Java, there is virtual memory so each thread can be allocated a 1 MB stack and not have to worry about running out of memory, even with a large number of threads.

Finally, it's really a pain to debug unsafe thread code. It's unpredictable and timing-sensitive. When you look for the bug it can stop happening.

### Say yes to finite state machines

[Finite state machines](https://en.wikipedia.org/wiki/Finite-state_machine) are a much better paradigm for memory and processor constrained devices like the Particle devices. There's only one stack, and no need to worry about thread concurrency.

Platforms like node.js work in a single-threaded environment using finite state machines or chained callbacks. This is a better model and even though the Particle platform is C++ instead of Javascript, the model works the same way.

### But I really want to use threads

OK. You've been warned. Here we go. This information is unofficial and subject to change.

## Using Threads

A bit of background:

- Threads are based on FreeRTOS (currently) but there is abstraction layer over it in case this changes.
- Threads are preemptively scheduled.
- A threads that yields will be called up to 1000 times per second (1 millisecond interval)
- Most API calls are not thread safe. 
- Basic synchronization capabilities exist, including mutex, recursive mutex, and queues.
- Threads are not supported on the Spark Core.

### How fast does a thread run?

Here's a simple example of implementing a thread to increment a counter in a loop.

```
#include "Particle.h"

SYSTEM_THREAD(ENABLED);

void threadFunction(void);

Thread thread("testThread", threadFunction);

volatile int counter = 0;
unsigned long lastReport = 0;

void setup() {
	Serial.begin(9600);
}

void loop() {
	if (millis() - lastReport >= 1000) {
		lastReport = millis();

		Serial.printlnf("counter=%d", counter);
	}
}


void threadFunction(void) {
	while(true) {
		counter++;
	}
	// You must not return from the thread function
}
```

The counter value is incremented in a tight loop and printed once per second from the main loop and note how fast it increments! Each line of the log is 1 second.

```
counter=131861864
counter=141750524
counter=151636087
counter=161521997
counter=171402702
counter=181294657
counter=191172552
counter=201055887
```

### With yield

A much better idea is to yield the CPU when you're done instead of crazy looping like that. Here's my modified threadFunction:

```
void threadFunction(void) {
	while(true) {
		counter++;

		os_thread_yield();
	}
	// You must not return from the thread function
}
```

Now the counter increments at a more sane rate, about 1000 calls per second, same as loop (on the Photon).

```
counter=3161
counter=4180
counter=4792
counter=5794
counter=6787
counter=7789
```

### Periodic scheduled calls

It's also possible to schedule periodic calls. In this case, we schedule the thread to execute every 10 milliseconds (100 times per second):

```
#include "Particle.h"

SYSTEM_THREAD(ENABLED);

void threadFunction(void *param);

Thread thread("testThread", threadFunction);

volatile int counter = 0;
unsigned long lastReport = 0;
system_tick_t lastThreadTime = 0;

void setup() {
	Serial.begin(9600);
}

void loop() {
	if (millis() - lastReport >= 1000) {
		lastReport = millis();

		Serial.printlnf("counter=%d", counter);
	}
}


void threadFunction(void *param) {
	while(true) {
		counter++;

		// Delay so we're called every 10 milliseconds (100 times per second)
		os_thread_delay_until(&lastThreadTime, 10);
	}
	// You must not return from the thread function
}

```

In the serial monitor you'll also note how much more regular the counts are this way:

```
counter=300
counter=400
counter=500
counter=600
```

### Synchronized access

System resources are not thread-safe and you must manually manage synchronization.

For example, the USB serial debug port (Serial) can only be called safely from multiple threads if you surround all accesses with WITH_LOCK(), as in:

```
#include "Particle.h"

SYSTEM_THREAD(ENABLED);

void threadFunction(void *param);

Thread thread("testThread", threadFunction);

volatile int counter = 0;
unsigned long lastReport = 0;
system_tick_t lastThreadTime = 0;

void setup() {
	Serial.begin(9600);
}

void loop() {
	if (millis() - lastReport >= 1000) {
		lastReport = millis();

		WITH_LOCK(Serial) {
			Serial.printlnf("counter=%d", counter);
		}
	}
}


void threadFunction(void *param) {
	while(true) {
		WITH_LOCK(Serial) {
			Serial.print(".");
		}
		counter++;

		// Delay so we're called every 100 milliseconds (10 times per second)
		os_thread_delay_until(&lastThreadTime, 100);
	}
	// You must not return from the thread function
}
```

Serial output:

```
.......counter=30
..........counter=40
..........counter=50
..........counter=60
..........counter=70
..........counter=80
..........counter=90
..........counter=100
```

Note that you must add WITH_LOCK in both your thread AND in the loop thread (and any software timers).

### Using a mutex to block a thread

One handy trick is to use a mutex to block your thread until something happens elsewhere. In this example, a SETUP/MODE button click handler can unblock the thread to make one run.

```
#include "Particle.h"

SYSTEM_THREAD(ENABLED);

void startupFunction();
void threadFunction(void *param);

// The mutex is initialized in startupFunction()
STARTUP(startupFunction());

Thread thread("testThread", threadFunction);

os_mutex_t mutex;


void buttonHandler();

void setup() {
	Serial.begin(9600);

	System.on(button_click, buttonHandler);
}

void loop() {
}

void buttonHandler() {
	// Release the thread mutex
	os_mutex_unlock(mutex);
}

// Note: threadFunction will be called before setup(), so you can't initialize the mutex there!
// STARTUP() is a good place to do it
void startupFunction() {
	// Create the mutex
	os_mutex_create(&mutex);

	// Initially lock it, so when the thread tries to lock it, it will block.
	// It's unlocked in buttonHandler()
	os_mutex_lock(mutex);
}

void threadFunction(void *param) {
	while(true) {
		// Block until unlocked by the buttonHandler
		os_mutex_lock(mutex);

		WITH_LOCK(Serial) {
			Serial.println("thread called!");
		}
	}
	// You must not return from the thread function
}

```

### Reading serial from a thread

One problem with the hardware UART serial is limited buffer size. One workaround for this is to read it from a thread. In this example it reads the USB serial just because it's easier to test.

The thread reads data from the serial port and buffers it until it gets a full line. Then it makes a copy of the data and puts it in a queue. The queue is read out of loop(), but the serial port is continuously read even if main is blocked.

```
#include "Particle.h"

SYSTEM_THREAD(ENABLED);

void threadFunction(void *param);

Thread thread("testThread", threadFunction);

// Instead of using STARTUP() another good way to initialize the queue is to use a lambda.
// setup() is too late.
os_queue_t queue = []() {
	os_queue_t q;
	// 20 is the maximum number of items in the queue.
	os_queue_create(&q, sizeof(void*), 20, 0);
	return q;
}();

system_tick_t lastThreadTime = 0;
char serialBuf[512];
size_t serialBufOffset = 0;

void setup() {
	Serial.begin(9600);
}

void loop() {
	// Try to take an item from the queue. First 0 is the amount of time to wait, 0 = don't wait.
	// Second 0 is the reserved value, always 0.
	char *s = 0;
	if (os_queue_take(queue, &s, 0, 0) == 0) {
		// We got a line of data by serial. Handle it here.
		// s is a copy of the data that must be freed when done.
		Serial.println(s);
		free(s);
	}
}


void threadFunction(void *param) {
	while(true) {
		while(Serial.available()) {
			char c = Serial.read();
			if (c == '\n') {
				// null terminate
				serialBuf[serialBufOffset] = 0;

				// Make a copy of the serialBuf
				char *s = strdup(serialBuf);
				if (s) {
					if (os_queue_put(queue, (void *)&s, 0, 0)) {
						// Failed to put into queue (queue full), discard the data
						free(s);
					}
				}

				// Clear buffer
				serialBufOffset = 0;
			}
			else
			if (serialBufOffset < (sizeof(serialBuf) - 1)) {
				// Add to buffer
				serialBuf[serialBufOffset++] = c;
			}
		}

		// Delay so we're called every 1 millisecond (1000 times per second)
		os_thread_delay_until(&lastThreadTime, 1);
	}
	// You must not return from the thread function
}
```

### Thread pools

This example is a thread pool. Say you have an operation that takes a variable amount of time to run. You want to run these operations on one or more worker threads. The operations are put in a queue, so you can queue up operations until a thread is available to run it. The queueing operation is fast, so it won't block the thread you call it from.

There's more code to this in the Github as the thread pool is implemented as a class. However, this is how it's used:

```
#include "Particle.h"

#include "ThreadPool.h"

SerialLogHandler logHandler;

// Create a pool of 2 threads and 10 call entries in the call queue
ThreadPool pool(2, 10);
volatile int lastCallNum = 0;


void buttonHandler();

void setup() {
	Serial.begin(9600);
	System.on(button_click, buttonHandler);
}

void loop() {
}

// This function is called when the SETUP/MODE button is pressed
void buttonHandler() {
	// When the button is pressed run a function that takes a random amount of time to complete, from 0 to 5 seconds.
	int callNum = lastCallNum++;

	// In 0.7.0 at least, Log.info from a system event handler doesn't do anything. You won't
	// see this log message.
	Log.info("thread call %d queued", callNum);

	// This is a C++11 lambda. The code in the {} block is executed later, in a separate thread.
	// It also has access to the callNum variable declared above.
	pool.callOnThread([callNum]() {
		// The code is this block run on a separate thread. You'll see these log messages.
		int fakeRunTime = rand() % 5000;
		Log.info("thread call %d started fakeRunTime=%d", callNum, fakeRunTime);

		// You'd normally actually do something useful here other than delay. This
		// is to simulate some tasks that takes a variable amount of time.
		delay(fakeRunTime);

		Log.info("thread call %d done", callNum);
	});
}
```

The important part is pool.callOnThread. This queues up a call and the code within the {} block is execute later.

Here's a sample output:

```
0000007325 [app] INFO: thread call 0 started fakeRunTime=933
0000008258 [app] INFO: thread call 0 done
0000117385 [app] INFO: thread call 1 started fakeRunTime=2743
0000120128 [app] INFO: thread call 1 done
0000120275 [app] INFO: thread call 2 started fakeRunTime=1262
0000120585 [app] INFO: thread call 3 started fakeRunTime=1529
0000121537 [app] INFO: thread call 2 done
0000121537 [app] INFO: thread call 4 started fakeRunTime=4700
0000122114 [app] INFO: thread call 3 done
0000122114 [app] INFO: thread call 5 started fakeRunTime=508
0000122622 [app] INFO: thread call 5 done
0000126237 [app] INFO: thread call 4 done
```

### Threaded TCPClient

In the [asynctcpclient](https://github.com/rickkas7/asynctcpclient) project, threads are used to make the connect() method of the TCPClient class asynchronous.

## More Details

You can find more documentation in the source.


- [spark\_wiring\_thread.h](https://github.com/particle-iot/firmware/blob/develop/wiring/inc/spark_wiring_thread.h) contains the headers for the Thread class.
- [concurrent\_hal.h](https://github.com/particle-iot/firmware/blob/develop/hal/inc/concurrent_hal.h) contains the headers for the low-level functions.
- [concurrent\_hal.cpp](https://github.com/particle-iot/firmware/blob/develop/hal/src/stm32f2xx/concurrent_hal.cpp) contains the implementations of the low-level functions so you can see how they map to FreeRTOS function.
- [FreeRTOS docs](https://www.freertos.org/a00106.html) are helpful as well.

Note that if you are browsing the concurrent_hal not all functions are exported to user firmware. In particular, you cannot use these functions from user firmware:

- os\_condition\_variable
- os\_semaphore

