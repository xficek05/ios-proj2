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
}semaphores;
//***********************

#define smphSIZE sizeof(semaphores)

//process for oxygen
//***********************
void oxygen_proc(semaphores *semaphore, int *cnt, int *o_cnt, int time)
{
	srand(getpid());
        int sleeptime = rand()%time+1;

	sem_wait(&(semaphore->mutex));
	
	int id = ++(*o_cnt);
	*cnt = *cnt + 1;

	//correct print
	printf("%d: O %d: started\n", *cnt, id);
	
	//debug print
	//printf("%d: O %d: started, sleep: %d, pid: %d, ppid: %d\n", *cnt, id, sleeptime, getpid(), getppid());
	fflush(stdout);

	sem_post(&(semaphore->mutex));
		
	usleep(sleeptime);

	sem_wait(&(semaphore->mutex));
	
	*cnt = *cnt + 1;

	printf("%d: O %d: going to queue\n", *cnt, id);
	
	fflush(stdout);

	sem_post(&(semaphore->mutex));
	
	exit(0);
}
//***********************

//process for hydrogen
//***********************
void hydrogen_proc(semaphores *semaphore, int *cnt, int *h_cnt, int time)
{
	srand(getpid());
        int sleeptime = rand()%time+1;

        sem_wait(&(semaphore->mutex));

        int id = ++(*h_cnt);
        *cnt = *cnt + 1;

	//correct print
	printf("%d: H %d: started\n", *cnt, id);

	//debug print
        //printf("%d: H %d: started, sleep: %d, pid: %d, ppid: %d\n", *cnt, id, sleeptime, getpid(), getppid());
        fflush(stdout);

        sem_post(&(semaphore->mutex));
	
	usleep(sleeptime);

	sem_wait(&(semaphore->mutex));
	
	*cnt = *cnt + 1;

	printf("%d: H %d: going to queue\n", *cnt, id);
 	fflush(stdout);

	sem_post(&(semaphore->mutex));

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

	//int molecule_time;
	//molecule_time = atoi(argv[4]);

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

	semaphores *semaphore;
	semaphore = mmap(NULL, smphSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_init(&(semaphore->mutex), 1, 1);

	pid_t pid = getpid();

	for (int i=0; i<oxygen; i++)
	{
		if (pid != 0)
		{
			pid = fork();
		}
		if (pid == 0)
		{
			oxygen_proc(semaphore, cnt, o_cnt, atom_time);
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
                        hydrogen_proc(semaphore, cnt, h_cnt, atom_time);
                }
        }

	for (int i=0; i<hydrogen+oxygen; i++)
	{
		wait(NULL);
	}


	

	return 0;
}
//***********************
