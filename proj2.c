//author: Timotej Halenar
//xlogin: xhalen00
//date: 28.4.2022

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

//barrier struct
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

//atom counters
//***********************
typedef struct
{
	int o;
	int h;
}queue_t;
//***********************

#define qSIZE sizeof(queue_t)

//***********************

//proj2.out
FILE *file;

//process for oxygen
//***********************
void oxygen_proc(semaphores *semaphore, int *cnt, int *o_cnt, int time, int moltime, queue_t *queue, int *mol_cnt, int *mol_three, int *max_mols, barrier_t *barrier)
{
	srand(getpid());
	//sleep time for atoms
        int sleeptime = rand()%time*1000;
	srand(getpid());
	//sleeptime for molecules
	int molsleeptime = rand()%moltime*1000;

	//wait for mutex token
	sem_wait(&(semaphore->mutex));
	
	//identificator
	int id = ++(*o_cnt);
	*cnt = *cnt + 1;

	fprintf(file, "%d: O %d: started\n", *cnt, id);
	
	sem_post(&(semaphore->mutex));
	
	//sleep, then go to queue	
	usleep(sleeptime);

	sem_wait(&(semaphore->mutex));
	
	*cnt = *cnt + 1;

	fprintf(file, "%d: O %d: going to queue\n", *cnt, id);

	//increment number of oxygens in queue
	queue->o  = queue->o + 1;

	//only make more molecules if they can be created
	if (*mol_cnt <= *max_mols)
	{
	//if there are two hydrogens waiting, let them through
	if (queue->h >= 2)
	{
		sem_post(&(semaphore->queue_h));
		sem_post(&(semaphore->queue_h));
		queue->h = queue->h - 2;
		sem_post(&(semaphore->queue_o));
		queue->o = queue->o - 1;

	}
	//if not, move on
	else
	{
		sem_post(&(semaphore->mutex));
	}
	}

	//waiting to be let through
	sem_wait(&(semaphore->queue_o));
	
	*cnt = *cnt + 1;

	//if all molecules have been created already, signal semaphores and exit
	if (*mol_cnt > *max_mols)
	{
		fprintf(file, "%d: O %d: not enough H\n", *cnt, id);
		//printf("gOt here\n");
		sem_post(&(semaphore->queue_o));
		sem_post(&(semaphore->mutex));
		exit(0);
	}

	fprintf(file, "%d: O %d: creating molecule %d\n", *cnt, id, *mol_cnt);

	//i don't know if this usleep does anything
	usleep(molsleeptime);

	//***barrier.wait start***
	sem_wait(&(barrier->mutex));
	barrier->count = barrier->count + 1;
	if (barrier->count == barrier->n)
	{
		sem_wait(&(barrier->turnstile2));
		sem_post(&(barrier->turnstile1));
	}
	sem_post(&(barrier->mutex));

	sem_wait(&(barrier->turnstile1));
	sem_post(&(barrier->turnstile1));

	//***critical section start***

	*cnt = *cnt + 1;	
	fprintf(file, "%d: O %d: molecule %d created\n", *cnt, id, *mol_cnt);

	//my elaborate molecule incrementing system, there's probably an easier way but this is what I've come up with
	*mol_three = *mol_three + 1;

	if (*mol_three == 3)
	{
		*mol_cnt = *mol_cnt + 1;
		*mol_three = 0;
	}

	//if this was the last molecule, open semaphores to let all other atoms through
	//these will get caught by a condition and killed
	if (*mol_cnt > *max_mols)
	{
		for (int i=0; i<*cnt; i++)
		{
			sem_post(&(semaphore->queue_o));
		}
		for (int i=0; i<*cnt; i++)
		{
			sem_post(&(semaphore->queue_h));
		}
	}
		
	//***critical section end***
	
	sem_wait(&(barrier->mutex));
	barrier->count = barrier->count - 1;
	if (barrier->count == 0)
	{
		sem_wait(&(barrier->turnstile1));
		sem_post(&(barrier->turnstile2));
	}
	sem_post(&(barrier->mutex));
	
	sem_wait(&(barrier->turnstile2));
	sem_post(&(barrier->turnstile2));

	///***barrier.wait end***

	sem_post(&(semaphore->mutex));

	exit(0);
}
//***********************

//process for hydrogen
//***********************
void hydrogen_proc(semaphores *semaphore, int *cnt, int *h_cnt, int time, queue_t *queue, int *mol_cnt, int *mol_three, int *max_mols, barrier_t *barrier)
{
	//same as the oxygen function
	srand(getpid());
        int sleeptime = rand()%time*1000;

        sem_wait(&(semaphore->mutex));

        int id = ++(*h_cnt);
        *cnt = *cnt + 1;

	fprintf(file, "%d: H %d: started\n", *cnt, id);

        sem_post(&(semaphore->mutex));
	
	usleep(sleeptime);

	sem_wait(&(semaphore->mutex));
	
	*cnt = *cnt + 1;

	fprintf(file, "%d: H %d: going to queue\n", *cnt, id);

	queue->h = queue->h + 1;

	if (*mol_cnt <= *max_mols)
	{
	//similar to oxygen, but checking the oxygen queue as well,
	//because we've just added another hydrogen, not oxygen
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
	}

	sem_wait(&(semaphore->queue_h));
	*cnt = *cnt + 1;

	if (*mol_cnt > *max_mols)
	{
		fprintf(file, "%d: H %d: not enough O or H\n", *cnt, id);
		sem_post(&(semaphore->mutex));
		sem_post(&(semaphore->queue_h));
		exit(0);
	}

	fprintf(file, "%d: H %d: creating molecule %d\n", *cnt, id, *mol_cnt);

	//***barrier.wait start***
	sem_wait(&(barrier->mutex));
	barrier->count = barrier->count + 1;
	if (barrier->count == barrier->n)
	{
		sem_wait(&(barrier->turnstile2));
		sem_post(&(barrier->turnstile1));
	}
	sem_post(&(barrier->mutex));

	sem_wait(&(barrier->turnstile1));
	sem_post(&(barrier->turnstile1));

	//***critical section start***
	
	*cnt = *cnt + 1;
	fprintf(file, "%d: H %d: molecule %d created\n", *cnt, id, *mol_cnt);

	*mol_three = *mol_three + 1;

	if (*mol_three == 3)
	{
		*mol_cnt = *mol_cnt + 1;
		*mol_three = 0;
	}
	
	if (*mol_cnt > *max_mols)
	{
		for (int i=0; i<*cnt; i++)
		{
			sem_post(&(semaphore->queue_o));
		}
		for (int i=0; i<*cnt; i++)
		{
			sem_post(&(semaphore->queue_h));
		}
	}
		
	//***critical section end***
	
	sem_wait(&(barrier->mutex));
	barrier->count = barrier->count - 1;
	if (barrier->count == 0)
	{
		sem_wait(&(barrier->turnstile1));
		sem_post(&(barrier->turnstile2));
	}
	sem_post(&(barrier->mutex));
	
	sem_wait(&(barrier->turnstile2));
	sem_post(&(barrier->turnstile2));

	///***barrier.wait end***

        exit(0);
}
//***********************

//calculate how many molecules will be created
//***********************
int mol_count(int oxygen, int hydrogen)
{
	return ((2*oxygen) < hydrogen) ? oxygen : (hydrogen/2);
}

//main
//***********************
int main(int argc, char *argv[])
{

	//argument checking	
	if (argc != 5)
	{
		fprintf(stderr, "Invalid arguments.\n");
		return 1;
	}

	if (atoi(argv[3]) > 1000)
	{
		fprintf(stderr, "Time argument out of range.\n");
		return 1;
	}
	
	if (atoi(argv[4]) > 1000)
	{
		fprintf(stderr, "Time argument out of range.\n");
		return 1;
	}

	//arguments parsing
	int oxygen;
	oxygen = atoi(argv[1]);

	int hydrogen;
	hydrogen = atoi(argv[2]);

	int atom_time;
	atom_time = atoi(argv[3]);

	int molecule_time;
	molecule_time = atoi(argv[4]);

	//opening file
	if ((file = fopen("proj2.out", "w")) == NULL)
	{
		fprintf(stderr, "Unable to open file.\n");
		return 1;
	}

	setbuf(file, NULL);
	setbuf(stderr, NULL);

	//shared variables
	//action counter
	int *cnt;
	cnt = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*cnt = 0;
	
	//oxygen counter
	int *o_cnt;
	o_cnt = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*o_cnt = 0;

	//hydrogen counter
	int *h_cnt;
	h_cnt = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*h_cnt = 0;

	//molecule counter
	int *mol_cnt;
	mol_cnt = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*mol_cnt = 1;

	//additional variable to accurately index molecules
	int *mol_three;
	mol_three = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*mol_three = 0;

	//number of molecules to be created
	int *max_mols;
	max_mols = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*max_mols = mol_count(oxygen, hydrogen);

	//number of O and H atoms in queue
	queue_t *queue;
	queue = mmap(NULL, qSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	queue->o = 0;
	queue->h = 0;

	//semaphores
	semaphores *semaphore;
	semaphore = mmap(NULL, smphSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_init(&(semaphore->mutex), 1, 1);
	sem_init(&(semaphore->queue_o), 1, 0);
	sem_init(&(semaphore->queue_h), 1, 0);

	//barrier
	barrier_t *barrier;
	barrier = mmap(NULL, brrSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_init(&(barrier->mutex), 1, 1);
	sem_init(&(barrier->turnstile1), 1, 0);
	sem_init(&(barrier->turnstile2), 1, 1);
	barrier->n = 3;
	barrier->count = 0;

	pid_t pid = getpid();

	//creating oxygen processes
	for (int i=0; i<oxygen; i++)
	{
		if (pid != 0)
		{
			pid = fork();
		}
		if (pid == 0)
		{
			oxygen_proc(semaphore, cnt, o_cnt, atom_time, molecule_time, queue, mol_cnt, mol_three, max_mols, barrier);
		}
	}

	pid = getpid();
	
	//creating hydrogen processes
	for (int i=0; i<hydrogen; i++)
        {
                if (pid != 0)
                {
                        pid = fork();
                }
                if (pid == 0)
                {
                        hydrogen_proc(semaphore, cnt, h_cnt, atom_time, queue, mol_cnt, mol_three, max_mols,  barrier);
                }
        }

	//waiting for children to die
	for (int i=0; i<hydrogen+oxygen; i++)
	{
		wait(NULL);
	}

	//cleanup	
	sem_destroy(&(semaphore->mutex));
	sem_destroy(&(semaphore->queue_o));
	sem_destroy(&(semaphore->queue_h));
	sem_destroy(&(barrier->mutex));
	sem_destroy(&(barrier->turnstile1));
	sem_destroy(&(barrier->turnstile2));

	munmap(cnt, intSIZE);
	munmap(o_cnt, intSIZE);
	munmap(h_cnt, intSIZE);
	munmap(mol_cnt, intSIZE);
	munmap(mol_three, intSIZE);
	munmap(max_mols, intSIZE);
	munmap(queue, qSIZE);
	munmap(semaphore, smphSIZE);
	munmap(barrier, brrSIZE);

	fclose(file);

	return 0;
}
//***********************
