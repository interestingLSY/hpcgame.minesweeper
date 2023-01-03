#include "queue.h"

void Queue::clear() {
	head = 0;
	tail = -1;
}

void Queue::push(int r, int c) {
	tail += 1;
	q[tail][0] = r;
	q[tail][1] = c;
}

void Queue::pop(int &r, int &c) {
	r = q[head][0];
	c = q[head][1];
	head += 1;
}

bool Queue::empty() {
	return head > tail;
}
