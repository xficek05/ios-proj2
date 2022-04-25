#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <semaphore.h>

#define intSIZE sizeof(int)

//semaphore struct
//***********************
typedef struct
{
	sem_t mutex;
	sem_t queue_h;
	sem_t queue_o;
}semaphores;
//***********************

#define smphSIZE sizeof(semaphores)

//***********************
typedef struct
{
	sem_t mutex;
	sem_t turnstile1;
	sem_t turnstile2;
	int n;
	int count;
}barrier_t;

//***********************

#define brrSIZE sizeof(barrier_t)

//***********************

typedef struct
{
	int o;
	int h;
	int *o_ids;
	int *h_ids;
}queue_t;

//***********************

#define qSIZE sizeof(queue_t)

//***********************
//process for oxygen
//***********************
void oxygen_proc(semaphores *semaphore, int *cnt, int *o_cnt, int time, int moltime, queue_t *queue, int *mol_cnt, barrier_t *barrier)
{
	(void) mol_cnt;
	srand(getpid());
        int sleeptime = rand()%time;
	srand(getpid());
	int molsleeptime = rand()%moltime;
	(void) molsleeptime;

	sem_wait(&(semaphore->mutex));
	
	int id = ++(*o_cnt);
	*cnt = *cnt + 1;

	//correct print
	printf("%d: O %d: started\n", *cnt, id);
	fflush(stdout);
	
	//debug print
	//printf("%d: O %d: started, sleep: %d, pid: %d, ppid: %d\n", *cnt, id, sleeptime, getpid(), getppid());
	fflush(stdout);

	sem_post(&(semaphore->mutex));
		
	usleep(sleeptime);

	sem_wait(&(semaphore->mutex));
	
	*cnt = *cnt + 1;

	printf("%d: O %d: going to queue\n", *cnt, id);	
	fflush(stdout);

	queue->o  = queue->o + 1;

	queue->o_ids[queue->o-1] = id;

	if (queue->h >= 2)
	{
		sem_post(&(semaphore->queue_h));
		sem_post(&(semaphore->queue_h));
		queue->h = queue->h - 2;
		sem_post(&(semaphore->queue_o));
		queue->o = queue->o - 1;

	}
	else
	{
		sem_post(&(semaphore->mutex));
	}

	sem_wait(&(semaphore->queue_o));
	*cnt = *cnt + 1;
	printf("%d: O %d: creating molecule 69\n", *cnt, id);
	fflush(stdout);

	//usleep(molsleeptime);

	//***barrier.wait start***
	sem_wait(&(barrier->mutex));
	barrier->count = barrier->count + 1;
	if (barrier->count == barrier->n)
	{
		sem_post(&(barrier->turnstile1));
		sem_post(&(barrier->turnstile1));
		sem_post(&(barrier->turnstile1));
	}
	sem_post(&(barrier->mutex));

	sem_wait(&(barrier->turnstile1));

	//***critical section start***
	
	printf("O %d: molecule 69 finished\n", id);
	fflush(stdout);
	
	//***critical section end***
	
	sem_wait(&(barrier->mutex));
	barrier->count = barrier->count - 1;
	if (barrier->count == 0)
	{
		sem_post(&(barrier->turnstile2));
		sem_post(&(barrier->turnstile2));
		sem_post(&(barrier->turnstile2));
	}
	sem_post(&(barrier->mutex));
	
	sem_wait(&(barrier->turnstile2));

	///***barrier.wait end***

	sem_post(&(semaphore->mutex));

	exit(0);
}
//***********************

//process for hydrogen
//***********************
void hydrogen_proc(semaphores *semaphore, int *cnt, int *h_cnt, int time, queue_t *queue, int *mol_cnt, barrier_t *barrier)
{
	(void) mol_cnt;
	srand(getpid());
        int sleeptime = rand()%time+1;

        sem_wait(&(semaphore->mutex));

        int id = ++(*h_cnt);
        *cnt = *cnt + 1;

	//correct print
	printf("%d: H %d: started\n", *cnt, id);
	fflush(stdout);

	//debug print
        //printf("%d: H %d: started, sleep: %d, pid: %d, ppid: %d\n", *cnt, id, sleeptime, getpid(), getppid());
        fflush(stdout);

        sem_post(&(semaphore->mutex));
	
	usleep(sleeptime);

	sem_wait(&(semaphore->mutex));
	
	*cnt = *cnt + 1;

	printf("%d: H %d: going to queue\n", *cnt, id);
 	fflush(stdout);

	queue->h = queue->h + 1;

	queue->h_ids[queue->h-1] = id;

	if (queue->h >= 2 && queue->o >= 1)
	{
		sem_post(&(semaphore->queue_h));
		sem_post(&(semaphore->queue_h));
		queue->h = queue->h - 2;
		sem_post(&(semaphore->queue_o));
		queue->o = queue->o - 1;
	}
	else
	{
		sem_post(&(semaphore->mutex));
	}

	sem_wait(&(semaphore->queue_h));

	//***barrier.wait start***
	sem_wait(&(barrier->mutex));
	barrier->count = barrier->count + 1;
	if (barrier->count == barrier->n)
	{
		sem_post(&(barrier->turnstile1));
		sem_post(&(barrier->turnstile1));
		sem_post(&(barrier->turnstile1));
	}
	sem_post(&(barrier->mutex));

	sem_wait(&(barrier->turnstile1));

	//***critical section start***
	
	printf("H %d: molecule 69 finished\n", id);
	fflush(stdout);
	
	//***critical section end***
	
	sem_wait(&(barrier->mutex));
	barrier->count = barrier->count - 1;
	if (barrier->count == 0)
	{
		sem_post(&(barrier->turnstile2));
		sem_post(&(barrier->turnstile2));
		sem_post(&(barrier->turnstile2));
	}
	sem_post(&(barrier->mutex));
	
	sem_wait(&(barrier->turnstile2));

	///***barrier.wait end***

        exit(0);
}
//***********************

//main
//***********************
int main(int argc, char *argv[])
{
	(void)argc;
	
	int oxygen;
	oxygen = atoi(argv[1]);

	int hydrogen;
	hydrogen = atoi(argv[2]);

	int atom_time;
	atom_time = atoi(argv[3]);

	int molecule_time;
	molecule_time = atoi(argv[4]);

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	int *cnt;
	cnt = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*cnt=0;

	int *o_cnt;
	o_cnt = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*o_cnt=0;

	int *h_cnt;
	h_cnt = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*h_cnt=0;

	int *mol_cnt;
	mol_cnt = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*mol_cnt=0;

	queue_t *queue;
	queue = mmap(NULL, qSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	queue->o = 0;
	queue->h = 0;

	queue->o_ids = mmap(NULL, sizeof(int*)*(oxygen+1), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);


	queue->h_ids = mmap(NULL, sizeof(int*)*(hydrogen+1), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);


	semaphores *semaphore;
	semaphore = mmap(NULL, smphSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_init(&(semaphore->mutex), 1, 1);
	sem_init(&(semaphore->queue_o), 1, 0);
	sem_init(&(semaphore->queue_h), 1, 0);

	barrier_t *barrier;
	barrier = mmap(NULL, brrSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_init(&(barrier->mutex), 1, 1);
	sem_init(&(barrier->turnstile1), 1, 0);
	sem_init(&(barrier->turnstile2), 1, 1);
	barrier->n = 3;
	barrier->count = 0;

	pid_t pid = getpid();

	for (int i=0; i<oxygen; i++)
	{
		if (pid != 0)
		{
			pid = fork();
		}
		if (pid == 0)
		{
			oxygen_proc(semaphore, cnt, o_cnt, atom_time, molecule_time, queue, mol_cnt, barrier);
		}
	}

	pid = getpid();

	for (int i=0; i<hydrogen; i++)
        {
                if (pid != 0)
                {
                        pid = fork();
                }
                if (pid == 0)
                {
                        hydrogen_proc(semaphore, cnt, h_cnt, atom_time, queue, mol_cnt, barrier);
                }
        }

	for (int i=0; i<hydrogen+oxygen; i++)
	{
		wait(NULL);
	}


	

	return 0;
}
//***********************
