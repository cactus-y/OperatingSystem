#include "mlfq.h"
// Multiple Level Feedback Queue

void 
init_queue(struct queue *q)
{
	q -> front = 0;
	q -> rear = 0;
}

int 
isEmpty(struct queue *q)
{
	if(q -> front == q -> rear)
		return 1;
	return 0;
}

int 
isFull(struct queue *q) 
{
	if(q -> front == (q -> rear + 1) % 65)
		return 1;
	return 0;
}

void 
enqueue(struct queue *q, struct proc *p)
{
	if(isFull(q))
	{
		//
	}
	else
	{
		q -> rear = (q -> rear + 1) % 65;
		q -> arr[q -> rear] = p;
	}
}

void 
dequeue(struct queue *q) 
{
	if(isEmpty(q))
	{
		//
	}
	else
	{
		q -> front = (q -> front + 1) % 65;
	}
}
