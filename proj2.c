/*******************************
  Richard Ficek xficek05
 *******************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>


FILE *fp = NULL;
int *count;
int *cntO;
int *cntH;
int *cntqueO;
int *cntqueH;
int *cntMol;
int *allMolekus;
int *barNum;
int *barCount;
int *molTimes;

//struktra pro semafory použité v projektu
typedef struct{
	sem_t mutex;
    sem_t queO;
    sem_t queH;
    sem_t mutex2;
    sem_t barrier1;
    sem_t barrier2;
}semaphores;

//kontrola vstupních argumentů zdali jsou čísla
int jeCislo(char *cislo){
    int i = 0;
	while (cislo[i] != '\0') i++;
	for (int j = 0; j < i; j++){
		if (cislo[j] > 57 || cislo[j] < 48)
			return 0;
        }
	return 1;
}
//tohle je na konci programu a čeká dokud děti neumřou
void sleeping(int O, int H){
	int i =0;
    while (i < O+H){
        wait(NULL);
        i++;
    } 
}
//čištění zavírání  semaforů, souboru a sdílených proměnných
void cleanup(semaphores *semaphore){
    sem_destroy(&(semaphore->mutex));
	sem_destroy(&(semaphore->queO));
	sem_destroy(&(semaphore->queH));
	sem_destroy(&(semaphore->mutex2));
	sem_destroy(&(semaphore->barrier1));
	sem_destroy(&(semaphore->barrier2));


    munmap(semaphore, sizeof(semaphores));
	munmap(count, sizeof(int));
	munmap(cntO, sizeof(int));
	munmap(cntH, sizeof(int));
	munmap(cntMol, sizeof(int));
	munmap(allMolekus, sizeof(int));
    munmap(cntqueO, sizeof(int));
	munmap(cntqueH, sizeof(int));
	munmap(barNum, sizeof(int));
	munmap(barCount, sizeof(int));
    munmap(molTimes, sizeof(int));

	fclose(fp);
}
// zalložení sdílených proměnných
void set_variables(){
    count = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	cntO = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	cntH = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    cntqueO = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	cntqueH = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    cntMol = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	allMolekus = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    barNum = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	barCount = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    molTimes = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
}
// funkce sloužicí k tisku
void message(int id, char znak,int a){
    if  (a == 1){
        fprintf(fp,"%d: %c %d: started\n",*count,znak,id);
        fflush(fp);
    }
    else if  (a == 2){
        fprintf(fp,"%d: %c %d: going to queue\n",*count,znak,id);
        fflush(fp);
    }
    else if (a == 3){
        fprintf(fp, "%d: %c %d: not enough H\n", *count,znak, id);
        fflush(fp);
    }
    else if (a == 4){
        fprintf(fp, "%d: %c %d: creating molecule %d\n", *count,znak,id, *cntMol);
        fflush(fp);
    }
    else if (a == 5){
        fprintf(fp, "%d: %c %d: molecule %d created\n", *count, znak,id, *cntMol);
        fflush(fp);
    }
    else if (a == 6){
        fprintf(fp, "%d: %c %d: not enough O or H\n", *count, znak,id);
        fflush(fp);
    }
}
// pomocná pro vytvoření času k vytvoření molelkuly / atomu
void waiting(int t){
    if (t == 0){
        t = 1;
    }
    usleep(rand()%t*1000);
}

void odchyd(){

}

void barrier(semaphores *semaphore,int id, char znak){
    //--- Barrier start
    // inspirace z knížky https://greenteapress.com/semaphores/LittleBookOfSemaphores.pdf strana 40
    sem_wait(&(semaphore->mutex2));
        (*barCount) += 1;
        if((*barCount) == (*barNum)) {
            sem_wait(&(semaphore->barrier2));
            sem_post(&(semaphore->barrier1));
        }
    sem_post(&(semaphore->mutex2));

    sem_wait(&(semaphore->barrier1));
    sem_post(&(semaphore->barrier1));

    *count+=1;	
	message(id,znak,5);

    //tohle hlídá počet vypsaných created aby byly jen 3
    *molTimes += 1;
	if (*molTimes == *barNum){
		*cntMol += 1;
		*molTimes = 0;
	}
    //zde se odchytávají zbyle atomu co se nevlezou do molekuly
    if (*cntMol > *allMolekus){
		for (int i=0; i<*count; i++){
			sem_post(&(semaphore->queO));
			sem_post(&(semaphore->queH));
		}
	}

    sem_wait(&(semaphore->mutex2));
    (*barCount) -= 1;
    if((*barCount) == 0) {
        sem_wait(&(semaphore->barrier1));
        sem_post(&(semaphore->barrier2));
    }
    sem_post(&(semaphore->mutex2));

    sem_wait(&(semaphore->barrier2));
    sem_post(&(semaphore->barrier2));
    //--- Barrier end
}

//proces klyslík
void oxygenProcess(semaphores *semaphore,int TI,int TB){
    //po spuštění výpis started
    char znak = 'O';
    int id = ++(*cntO);
    *count+=1;
    message(id,znak,1); //started
    // následně čeká pomocí usleep náhodný čas v intervalu 0,TI, tvorba atomu
    waiting(TI);
    //výpis going to queue a zařadí se do fronty
    sem_wait(&(semaphore->mutex));
	*count+=1;
	message(id,znak,2);
    //přidání O do que
    *cntqueO+=1;

    if(*cntMol <= *allMolekus){
        if (*cntqueH > 2){
            sem_post(&(semaphore->queH));
            sem_post(&(semaphore->queH));
            *cntqueH -= 2;
            sem_post(&(semaphore->queO));
            *cntqueO -= 1;
        }
        else {
            sem_post(&(semaphore->mutex));
        }
    }
    
    sem_wait(&(semaphore->queO));
    //tisk a opatření not enough
    *count+=1;
    if (*cntMol > *allMolekus){      
        message(id,znak,3);
        sem_post(&(semaphore->queO));
		sem_post(&(semaphore->mutex));
		exit(0);
    }
    //tisk creating molecule
    message(id,znak,4);
    //čas pro tvorbu molekuly
    waiting(TB);
    
    barrier(semaphore,id,znak); 

    sem_post(&(semaphore->mutex));
    
	exit(0);
}

void hydrogenProcess(semaphores *semaphore,int TI){
    char znak = 'H';

    int id = ++(*cntH);
    *count+=1;
    message(id,znak,1);
    //tvorba atomu
    waiting(TI);
    
	*count+=1;
    sem_wait(&(semaphore->mutex));
	message(id,znak,2);
    *cntqueH+=1;

    if(*cntMol <= *allMolekus){
        if (*cntqueH >= 2 && *cntqueO >= 1){
            sem_post(&(semaphore->queH));
            sem_post(&(semaphore->queH));
            *cntqueH -= 2;
            sem_post(&(semaphore->queO));
            *cntqueO -= 1;
        }
        else {
            sem_post(&(semaphore->mutex));
        }
    }

    sem_wait(&(semaphore->queH));
    *count+=1;
    
    if (*cntMol > *allMolekus){
        message(id,znak,6);
		sem_post(&(semaphore->mutex));
        sem_post(&(semaphore->queH));
		exit(0);
    }

    message(id,znak,4);

    barrier(semaphore,id,znak);    
	exit(0);
}


int main(int argc, char **argv){
    //ověření vstupu
    int H;
    int O;
    int TI;
    int TB;
    if(argc == 5){
        if (jeCislo(argv[1]) && jeCislo(argv[2]) && jeCislo(argv[3]) && jeCislo(argv[4])){
            O = atoi(argv[1]);
	        H = atoi(argv[2]);
	        TI = atoi(argv[3]);
	        TB = atoi(argv[4]);
            if (!(0 <= TI && TI <= 1000) || !(0 <= TB && TB <= 1000) || O <= 0 || H <= 0 ){
                fprintf(stderr,"Error: One of the out of range.\n");
			    exit(1);
            }
        }
        else{
            fprintf(stderr,"Error: One of the arguments have wrong format.\n");
			exit(1);
        }
    }
    else{
            fprintf(stderr, "Error: Too few or many arguments.\n");
			exit(1);
    }

    if ((fp = fopen("proj2.out", "w")) == NULL)
	{
		fprintf(stderr, "Unable to open file.\n");
		return 1;
	}
	setbuf(fp, NULL);
	setbuf(stderr, NULL);

    //shared variable
    set_variables();
    //set shared variable
	*count = 0;
    *cntO = 0;
    *cntH = 0;
    *cntqueO = 0;
    *cntqueH = 0;
    *cntMol = 1;
    *allMolekus = H / 2;
    *barNum = 3;
    *barCount = 0;
    *molTimes = 0;

	// semaphores
    semaphores *semaphore;
    semaphore = mmap(NULL, sizeof(semaphores), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (sem_init(&(semaphore->mutex), 1, 1) == -1 ||
	    sem_init(&(semaphore->queO), 1, 0) == -1 ||
	    sem_init(&(semaphore->queH), 1, 0) == -1 ||
        sem_init(&(semaphore->mutex2), 1, 1) == -1 ||
	    sem_init(&(semaphore->barrier1), 1, 0) == -1 ||
	    sem_init(&(semaphore->barrier2), 1, 1)== -1 ){
            fprintf(stderr, "error: Semaphore initialization failed.\n");
            exit(1);
    }
    	
	pid_t pid = getpid();
    for (int i=0; i<O; i++){
		if (pid != 0){
			pid = fork();
		}
		if (pid == 0){
            oxygenProcess(semaphore,TI,TB);
		}
	}
    
    pid = getpid();
	for (int i=0; i<H; i++){
		if (pid != 0){
			pid = fork();
		}
		if (pid == 0){
			hydrogenProcess(semaphore,TI);
		}
	}
	
	sleeping(O,H);
	cleanup(semaphore);

    return 0;
}
