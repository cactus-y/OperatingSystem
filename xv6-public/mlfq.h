#include "proc.h"
// Multiple Level Feedback Queue
struct queue {
	int front;
	int rear;
	struct proc *arr[65];
};

void init_queue(struct queue *q);

int isEmpty(struct queue *q);

int isFull(struct queue *q);

void enqueue(struct queue *q, struct proc *p);

void dequeue(struct queue *q);
