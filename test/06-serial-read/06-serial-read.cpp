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
