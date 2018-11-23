#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include<sys/time.h>

#define MAX_QUEUE_SIZE_IN_BYTES			(1024)

#define MQ_SIZE_MAX 				512
#define MQ_LENGTH_MAX 				30
#define MQ_NAME 				"msg queue example"

typedef struct _simple_queue
{
	int front;
	int rear;
	int length;
	int queue_type;
	pthread_mutex_t data_mutex;
	pthread_cond_t data_cond;
	int write_pos;
	char queue_name[32];
	void *data[0];

}simple_queue;

typedef enum _queue_type
{
	QUEUE_BLOCK = 0,
	QUEUE_NO_BLOCK,
}queue_type;

typedef enum _queue_status
{
	QUEUE_IS_NORMAL = 0,
	QUEUE_NO_EXIST,
	QUEUE_IS_FULL,
	QUEUE_IS_EMPTY,
	
}queue_status;

typedef enum _cntl_queue_ret
{
	CNTL_QUEUE_SUCCESS = 0,
	CNTL_QUEUE_FAIL,
	CNTL_QUEUE_TIMEOUT,
	CNTL_QUEUE_PARAM_ERROR,
}cntl_queue_ret;

typedef enum _queue_flag
{
	IPC_BLOCK = 0,
	IPC_NOWAIT = 1,
	IPC_NOERROR = 2,
}queue_flag;

typedef struct _simple_queue_buf
{
	int msg_type;
	char msg_buf[50];
}queue_buf;
			

simple_queue* create_simple_queue(const char* queue_name, int queue_length, int queue_type)
{
	simple_queue *this = NULL;
	
	if (NULL == queue_name || 0 == queue_length)
	{
		printf("[%s] param is error\n", __FUNCTION__);
		return NULL;
	}
	if(queue_length > MAX_QUEUE_SIZE_IN_BYTES)
	{
		printf("[%s] param is error,queue_length should less than %d bytes\n", __FUNCTION__, MAX_QUEUE_SIZE_IN_BYTES);
		return NULL;
	}
	this = (simple_queue*)malloc(sizeof(simple_queue) + queue_length * sizeof(void*));
	if (NULL != this)
	{
		this->front = 0;
		this->rear = 0;
		this->length = queue_length;
		this->queue_type = queue_type;
		if (0 != pthread_mutex_init(&(this->data_mutex), NULL) || 0 != pthread_cond_init(&(this->data_cond), NULL))
		{
			printf("[%s]pthread_mutex_init failed!\n", __FUNCTION__);
			free(this);
			this = NULL;
			return NULL;
		}
		
		strcpy(this->queue_name, queue_name);
		
	}
	else
	{
		printf("[%s]malloc is failed!\n", __FUNCTION__);
		return NULL;
	}
	
	return this;
}

queue_status is_full_queue(simple_queue* p_queue)
{
	queue_status ret = QUEUE_IS_NORMAL;
	
	do
	{
		if (NULL == p_queue)
		{
			printf("[%s] param is error\n", __FUNCTION__);
			ret = QUEUE_NO_EXIST;
			break;
		}
		if (p_queue->front == ((p_queue->rear + 1) % (p_queue->length)))
		{
			printf("[%s] queue is full\n", __FUNCTION__);
			ret = QUEUE_IS_FULL;
			break;
		}
	}while(0);
	
	return ret;
}

queue_status is_empty_queue(simple_queue* p_queue)
{
	queue_status ret = QUEUE_IS_NORMAL;
	
	do
	{
		if (NULL == p_queue)
		{
			printf("[%s] param is error\n", __FUNCTION__);
			ret = QUEUE_NO_EXIST;;
			break;
		}
		if (p_queue->front == p_queue->rear)
		{
			printf("[%s] queue is empty\n", __FUNCTION__);
			ret = QUEUE_IS_EMPTY;
			break;
		}
	}while(0);
	
	return ret;
}

cntl_queue_ret push_simple_queue(simple_queue* p_queue, void* data, queue_flag flg)
{
	int w_cursor = 0;
	
	if(NULL == p_queue || NULL == data)
	{
		printf("[%s] param is error\n", __FUNCTION__);
		return CNTL_QUEUE_PARAM_ERROR;
	}
	
	pthread_mutex_lock(&(p_queue->data_mutex));
 
	w_cursor = (p_queue->rear + 1)%p_queue->length;
	if (w_cursor == p_queue->front)
	{

		if(flg == IPC_BLOCK)
		{
			pthread_cond_wait(&(p_queue->data_cond), &(p_queue->data_mutex));
		}
		else
		{
			printf("[%s]: queue is full\n", __FUNCTION__);	
			pthread_mutex_unlock(&(p_queue->data_mutex));		
			return CNTL_QUEUE_FAIL;
		}
		
		w_cursor = (p_queue->rear + 1)%p_queue->length;
	}
	p_queue->data[p_queue->rear] = data;
	p_queue->rear = w_cursor;
 
	pthread_mutex_unlock(&(p_queue->data_mutex));
	pthread_cond_signal(&(p_queue->data_cond));
	
	
	return CNTL_QUEUE_SUCCESS;
}

cntl_queue_ret pop_simple_queue(simple_queue* p_queue, void** data, queue_flag flg)
{
	if(NULL == p_queue)
	{
		printf("[%s] param is error\n", __FUNCTION__);
		return CNTL_QUEUE_PARAM_ERROR;
	}
 
	pthread_mutex_lock(&(p_queue->data_mutex));
	
	if (p_queue->front == p_queue->rear)
	{

		if(flg == IPC_BLOCK)
		{
			pthread_cond_wait(&(p_queue->data_cond), &(p_queue->data_mutex));
		}
		else
		{
			printf("[%s]: queue is empty\n", __FUNCTION__);
			pthread_mutex_unlock(&(p_queue->data_mutex));
			return CNTL_QUEUE_FAIL;
		}	

	}
	*data = p_queue->data[p_queue->front];
	p_queue->front = (p_queue->front + 1)%p_queue->length;
	
	pthread_mutex_unlock(&(p_queue->data_mutex));
	pthread_cond_signal(&(p_queue->data_cond));
	
	return CNTL_QUEUE_SUCCESS;
}

cntl_queue_ret destroy_simple_queue(simple_queue* p_queue)
{
	cntl_queue_ret ret = CNTL_QUEUE_SUCCESS;
	
	if(NULL == p_queue)
	{
		printf("[%s] param is error\n", __FUNCTION__);
		ret = CNTL_QUEUE_PARAM_ERROR;
	}
	else
	{
		pthread_mutex_destroy(&(p_queue->data_mutex));
		pthread_cond_destroy(&(p_queue->data_cond));
		while (p_queue->front != p_queue->rear)//删除队列中残留的消息
		{
			free(p_queue->data[p_queue->front]);
			p_queue->front = (p_queue->front + 1)%p_queue->length;
		}
		free(p_queue);
		p_queue = NULL;
	}
	
	return ret;
}

void* send_msg_thread(void* arg)
{
	queue_buf* send_buf = NULL;
	int i;
 
	send_buf = (queue_buf*)malloc(sizeof(queue_buf));
	send_buf->msg_type = 1;
	strcpy(send_buf->msg_buf, "hello, world!");

	printf("first1: rear =%d font =%d\n", ((simple_queue*)arg)->rear, ((simple_queue*)arg)->front);

	if (push_simple_queue((simple_queue*)arg, (void*)send_buf, IPC_BLOCK) < 0) 
	{ 
        	printf("[%s]: push_simple_queue\n", __FUNCTION__);  
        	return NULL; 
    	}
	printf("first2: rear =%d font =%d\n", ((simple_queue*)arg)->rear, ((simple_queue*)arg)->front);

	queue_buf* send_buf1 = NULL;
 
	send_buf1 = (queue_buf*)malloc(sizeof(queue_buf));
	send_buf1->msg_type = 2;
	strcpy(send_buf1->msg_buf, "byebye");
	
	printf("first1: rear =%d font =%d\n", ((simple_queue*)arg)->rear, ((simple_queue*)arg)->front);

	if (push_simple_queue((simple_queue*)arg, (void*)send_buf1, IPC_NOWAIT) < 0) 
	{ 
        	printf("[%s]: push_simple_queue\n", __FUNCTION__);  
        	return NULL; 
    	}
	printf("first2: rear =%d font =%d\n", ((simple_queue*)arg)->rear, ((simple_queue*)arg)->front);
	
	return NULL;

}

void* recv_msg_thread(void* arg)
{
	int i;

	queue_buf* recv_buf = (queue_buf*)malloc(sizeof(queue_buf));
	
	printf("second1 rear =%d font =%d\n", ((simple_queue*)arg)->rear, ((simple_queue*)arg)->front);

	if (CNTL_QUEUE_SUCCESS != pop_simple_queue((simple_queue*)arg, (void**)&recv_buf, IPC_BLOCK)) 
	{  
        	printf("[%s]: pop_simple_queue failed!\n", __FUNCTION__); 
		return NULL;
    	}
	for(i=0; i<50; i++)
		printf("%c", recv_buf->msg_buf[i]);
	printf("\r\n");

	printf("second2: rear =%d font =%d\n", ((simple_queue*)arg)->rear, ((simple_queue*)arg)->front);

	printf("second1: rear =%d font =%d\n", ((simple_queue*)arg)->rear, ((simple_queue*)arg)->front);

	if (CNTL_QUEUE_SUCCESS != pop_simple_queue((simple_queue*)arg, (void**)&recv_buf, IPC_NOWAIT))
	{  
        	printf("[%s]: pop_simple_queue failed!\n", __FUNCTION__); 
		return NULL;
    	}
	for(i=0; i<50; i++)
		printf("%c", recv_buf->msg_buf[i]);
	printf("\r\n");
	printf("second2: rear =%d font =%d\n", ((simple_queue*)arg)->rear, ((simple_queue*)arg)->front);
	
	free(recv_buf);
	recv_buf = NULL;

	return NULL;

}

int main(int argc, char* argv[])
{
	int ret = 0;
	pthread_t send_thread_id = 0;
	pthread_t recv_thread_id = 0;
	simple_queue* msg_queue = NULL;
	
	msg_queue = create_simple_queue(MQ_NAME, MQ_LENGTH_MAX, QUEUE_NO_BLOCK);
	if (NULL == msg_queue)
	{
		printf("[%s]: create simple queue failed!\n", __FUNCTION__);
		return -1;
	}
	ret = pthread_create(&send_thread_id, NULL, send_msg_thread, (void*)msg_queue);
	if (0 != ret)
	{
		printf("[%s]: create send thread failed!\n", __FUNCTION__);
		return -1;
	}
	ret = pthread_create(&recv_thread_id, NULL, recv_msg_thread, (void*)msg_queue);
	if (0 != ret)
	{
		printf("[%s]: create recv thread failed!\n", __FUNCTION__);
		return -1;
	}
	printf("begin join\n");
	pthread_join(send_thread_id, NULL);
	pthread_join(recv_thread_id, NULL);
	printf("end join\n");
	ret = destroy_simple_queue(msg_queue);
	if (CNTL_QUEUE_SUCCESS != ret)
	{
		printf("[%s]: destroy simple queue failed!\n", __FUNCTION__);
		return -1;
	}
	
	return 0;

}




