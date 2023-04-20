// #include "mlfq.h"
// // Multiple Level Feedback Queue

// void 
// init_queue(struct mlfq *mq) 
// {
// 	for(int i = 0; i < 3; i++) 
// 	{
// 		mq -> front[i] = 0;
// 		mq -> rear[i] = 0;
// 	}
// }

// int 
// isEmpty(struct mlfq *mq, int level)
// {
// 	if(mq -> front[level] == mq -> rear[level])
// 		return 1;
// 	return 0;
// }

// int 
// isFull(struct mlfq *mq, int level) 
// {
// 	if(mq -> front[level] == (mq -> rear[level] + 1) % 64)
// 		return 1;
// 	return 0;
// }

// void 
// enqueue(struct queue *mq, struct proc *p)
// {
// 	for(int i = 0; i < 3; i++) {
// 		if(isFull(mq, i)) 
// 		{
// 			continue;
// 		} 
// 		else 
// 		{
// 			mq -> rear[i] = (mq -> rear[i] + 1) % 64;
// 			mq -> arr[mq -> rear[i]] = p;
// 			return;
// 		}
// 	}
// 	// panic
// 	return;
// }

// void 
// dequeue(struct queue *q) 
// {
// 	if(isEmpty(q))
// 	{
// 		//
// 	}
// 	else
// 	{
// 		q -> front = (q -> front + 1) % 65;
// 	}
// }


