#include "Particle.h"

SYSTEM_THREAD(ENABLED);

void startupFunction();
void threadFunction(void *param);

// The mutex is initialized in startupFunction() because setup() is too late.
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
// STARTUP() is a good place to do it, though I prefer the method in example 07-serial-read using
// a lambda.
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
