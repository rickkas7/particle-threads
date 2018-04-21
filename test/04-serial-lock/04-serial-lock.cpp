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
