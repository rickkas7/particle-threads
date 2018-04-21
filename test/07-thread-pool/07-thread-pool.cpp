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
