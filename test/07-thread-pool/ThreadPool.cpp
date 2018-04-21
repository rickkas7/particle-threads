
#include "ThreadPool.h"

ThreadPoolThread::ThreadPoolThread(ThreadPool *threadPool, const char *name, os_thread_prio_t priority, size_t stackSize) : threadPool(threadPool){

    os_thread_create(&thread, name, priority, staticRun, (void *)this, stackSize);

}

ThreadPoolThread::~ThreadPoolThread() {

}

void ThreadPoolThread::run() {
	while(true) {
		threadPool->runCall();
	}
}


// static
void ThreadPoolThread::staticRun(void *param) {
	ThreadPoolThread *_this = (ThreadPoolThread *)param;
	_this->run();
}



ThreadPool::ThreadPool(size_t numThreads, size_t numCalls, os_thread_prio_t priority, size_t stackSize) : numThreads(numThreads) {

	// Create two queues, one for pending calls and one for free ThreadPoolCall objects
	os_queue_create(&callQueue, sizeof(void *), numCalls, 0);
	os_queue_create(&freeQueue, sizeof(void *), numCalls, 0);

	// Fill the free queue with blank objects
	for(size_t ii = 0; ii < numCalls; ii++) {
		ThreadPoolCall *c = new ThreadPoolCall();
		os_queue_put(freeQueue, &c, 0, 0);
	}

	// Create an array of ThreadPoolThread*
	threads = new ThreadPoolThread*[numThreads];

	// Initialize them
	for(size_t ii = 0; ii < numThreads; ii++) {
		char nameBuf[16];
		snprintf(nameBuf, sizeof(nameBuf), "pool%u", ii);
		threads[ii] = new ThreadPoolThread(this, nameBuf, priority, stackSize);
	}
}

ThreadPool::~ThreadPool() {

}

bool ThreadPool::callOnThread(std::function<void(void)> fn, system_tick_t delay) {
	ThreadPoolCall *c;

	if (os_queue_take(freeQueue, &c, delay, 0)) {
		return false;
	}
	c->fn = fn;

	// There will always be room because the queues are the equal to the total number of objects
	return os_queue_put(callQueue, &c, 0, 0) == 0;
}

void ThreadPool::runCall() {
	ThreadPoolCall *c;

	os_queue_take(callQueue, &c, CONCURRENT_WAIT_FOREVER, 0);

	c->fn();

	os_queue_put(freeQueue, &c, 0, 0);
}


//int os_queue_put(os_queue_t queue, const void* item, system_tick_t delay, void* reserved);


