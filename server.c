#include "server.h"

//Initialize and assign default values to port and dict
int connectionPort = DEFAULT_PORT;
char* dictName = DEFAULT_DICT;
char dictionary[MAX_WORDS][MAX_WORD_SIZE];

//Implementation of Queue 
char * log_buf [MAX_LOG_QUEUE][MESSAGE_SIZE];
int job_buf[MAX_JOB_QUEUE];

int job_len = MAX_LOG_QUEUE;
int log_len = MAX_JOB_QUEUE;

int job_count = 0;
int log_count = 0;
int job_front;
int job_rear;
int log_front;
int log_rear;
	
pthread_mutex_t job_mutex, log_mutex;
pthread_cond_t job_cv_cs, job_cv_pd; //Job buffer not empty, job buffer not full
pthread_cond_t log_cv_cs, log_cv_pd; // Log buffer not empty, log buffer not full

pthread_t worker_threads[NUM_WORKERS], log_thread;

int wordCount = 0;

char* clientMessage = "Connected to Networked Spell Checker. Send a word to spell check. Esc to disconnect.\n";
char* msgRequest = "Send another word or esc to quit.\n";
char* msgClose = "Disconnecting from server, Goodbye!.\n";


int main(int argc, char** argv)
{

	//Initialize pthread_t variables
	if(pthread_mutex_init(&job_mutex, NULL) != 0 ){
		printf("Error initializing job mutex. Program terminated.");
		exit(EXIT_FAILURE);
	}
	if(pthread_mutex_init(&log_mutex,NULL)!= 0){
		printf("Error initializing log mutex. Program terminated.");
		exit(EXIT_FAILURE);
	}

	if(pthread_cond_init(&job_cv_cs,NULL) != 0){
		printf("Error initializing job_cv_cs condition variable. Program terminated.");
		exit(EXIT_FAILURE);
	}
	if(pthread_cond_init(&job_cv_pd,NULL) != 0){
		printf("Error initializing job_cv_pd condition variable. Program terminated.");
		exit(EXIT_FAILURE);
	}
	if(pthread_cond_init(&log_cv_cs,NULL) != 0){
		printf("Error initializing log_cv_cs condition variable. Program terminated.");
		exit(EXIT_FAILURE);
	}
	if(pthread_cond_init(&log_cv_pd,NULL) != 0){
		printf("Error initializing log_cv_pd condition variable. Program terminated.");
		exit(EXIT_FAILURE);
	}


	if(argc == 1){
		printf("No port number or dictionary name entered. Using defaults (8888 and words.txt) respectively. \n");
	} 
	else if(argc == 2){
		// Port or Dic specified
		if (strstr(argv[1], ".txt") == NULL){
			connectionPort = atoi(argv[1]);
		} else{
			dictName = argv[1];
		}
	}
	else if(argc == 3){
		// [Port] then [Dict] specified
		connectionPort = atoi(argv[1]);
		dictName = argv[2];
	}
	else{
		printf("Invalid number of arguments.");
		return -1;
	}

	// Read in dictionary
	FILE* dictPtr = fopen(dictName, "r");
	if (!dictPtr){
		printf("Failed to open dictionary.");
		exit(EXIT_FAILURE);
	} 
	
	int i=0;
	while((fgets(dictionary[i], sizeof(dictionary[i]), dictPtr)!=NULL) && (i < MAX_WORDS - 1)){
		//fill dictionary buffer
		wordCount++;
		i++;
	}

	//Create threads
	for(int i = 0; i < NUM_WORKERS; i++){
		pthread_create(&worker_threads[i],NULL, workFunc,NULL);
	}
	pthread_create(&log_thread, NULL, logFunc, NULL);


	//TEST DICTIONARY READ IN
	/*
	printf("%s", dictionary[0]);
	printf("%s", dictionary[1]);
	printf("%s", dictionary[2]);
	printf("%s", dictionary[wordCount-1]);
	*/

	//Test spellcheck here
	//printf("Results for spellcheck: %d", spellCheck("aver"));
	//SPELLCHECK FUNCTION WORKS, CONSEQUENTLY SO DOES DICTIONARY READIN

	//sockaddr_in holds information about the user connection. 
	//We don't need it, but it needs to be passed into accept().
	struct sockaddr_in client;
	int clientLen = sizeof(client);
	int connectionSocket, clientSocket, bytesReturned;
	char recvBuffer[BUF_LEN];
	recvBuffer[0] = '\0';


	//We can't use ports below 1024 and ports above 65535 don't exist.
	if(connectionPort < 1024 || connectionPort > 65535){
		printf("Port number is either too low(below 1024), or too high(above 65535).\n");
		return -1;
	}

	//Does all the hard work for us.
	connectionSocket = open_listenfd(connectionPort);
	if(connectionSocket == -1){
		printf("Could not connect, maybe try another port number?\n");
		return -1;
	}

	//accept() waits until a user connects to the server, writing information about that server
	//into the sockaddr_in client.
	//If the connection is successful, we obtain A SECOND socket descriptor. 
	//There are two socket descriptors being used now:
	//One by the server to listen for incoming connections.
	//The second that was just created that will be used to communicate with 
	//the connected user.
	while(1){
		if((clientSocket = accept(connectionSocket, (struct sockaddr*)&client, &clientLen)) == -1){
			printf("Error connecting to client.\n");
			return -1;
		}

		pthread_mutex_lock(&job_mutex);
		while(job_count == MAX_JOB_QUEUE){
			pthread_cond_wait(&job_cv_pd, &job_mutex); //Wait for queue to not be full
		}

		if(job_count == MAX_JOB_QUEUE){
			printf("Job queue full.");
		} 
		else{
			if(job_count == 0){
				//First job being added to queue. Head and rear point to it.
				job_front = 0;
				job_rear = 0;
			}
			job_buf[job_rear] = clientSocket; //Add socket to end of queue
			job_count++;
			job_rear = (job_rear+1) % MAX_JOB_QUEUE;
		}

		pthread_mutex_unlock(&job_mutex);
		pthread_cond_signal(&job_cv_cs); //Signal queue has something in it
		printf("Connection success!\n");
		send(clientSocket, clientMessage, strlen(clientMessage), 0);
	}
	return 0;
}


void* workFunc(void* args){
	while(1){
		//Keep working
		pthread_mutex_lock(&job_mutex); // lock job mutex
		while(job_count == 0){
			//wait until job queue has things in it
			pthread_cond_wait(&job_cv_cs, &job_mutex);
		}

		int currentSocket;
		currentSocket = job_buf[job_front];
		job_front = (job_front + 1) & MAX_JOB_QUEUE;
		job_count--;
		
		pthread_mutex_unlock(&job_mutex);
		pthread_cond_signal(&job_cv_pd); //signal job queue not full

		//Handle a client's word
		char* currentString = calloc(MAX_WORD_SIZE, 1);
		//currentString = strtok(currentString, " \n\r");

		while(recv(currentSocket, currentString, MAX_WORD_SIZE, 0)){
			if(strlen(currentString) <= 1){
				continue; //skip rest of loop if string is just a newline or null
			}
			//Quit on esc key (check simple server code)
			if (currentString[0] == 27){
				printf("Quitting...");
				write(currentSocket, msgClose, strlen(msgClose));
				close(currentSocket);
				break; //stop executing
			}	

			//Test if reading correctly
			
			printf("String before modifying: %s\n", currentString);
			char temp[MAX_WORD_SIZE];
			strcpy(temp, currentString);
			printf("Temp: %s\n", temp);
			printf("Dictionary string: %s\n", dictionary[0]);
			if(strcmp(dictionary[0],temp)==0){
				printf("Test: Word's match\n");
			} else{
				printf("Test: Word's don't match\n");
			}
			
			if(spellCheck(currentString)){
				//Positive
				strtok(currentString," \n\r");
				currentString = realloc(currentString, sizeof(char*) * (MESSAGE_SIZE)); //currentString is of indeterminate size so need to realloc
				strcat(currentString, " CORRECT\n");
			} 
			else{
				//Negative
				//printf("%s", currentString);
				strtok(currentString," \n\r");
				currentString = realloc(currentString, sizeof(char*) * (MESSAGE_SIZE));
				strcat(currentString," INCORRECT\n");
				//TEST
				//printf("%s", currentString);
			}
			//printf("Log message: %s", currentString);
			write(currentSocket, currentString, strlen(currentString)); // Send spellcheck result
			write(currentSocket, msgRequest, strlen(msgRequest)); // Send followup request
			
			

			// Add spellcheck message to log queue
			pthread_mutex_lock(&log_mutex);
			while(log_count == MAX_LOG_QUEUE){
				pthread_cond_wait(&log_cv_pd, &log_mutex); //Wait until log is not full
			}

			if(log_count == MAX_LOG_QUEUE){
				//Check if log queue full
			} 
			else{
				if(log_count == 0){
					//Initialize front and rear of log queue to 0
					log_front = 0;
					log_rear = 0;
				}
				strcpy(log_buf[log_rear], currentString); // add spellcheck message to log queue
				log_count++;
				log_rear = (log_rear + 1) % MAX_LOG_QUEUE;
			}
			pthread_mutex_unlock(&log_mutex);
			pthread_cond_signal(&log_cv_cs); //signal log queue not empty

			free(currentString);
			currentString = calloc(MAX_WORD_SIZE, 1);
		}
		close(currentSocket);
	}
}

void* logFunc(void* args){
	while(1){
		pthread_mutex_lock(&log_mutex); //lock log mutex
		while(log_count == 0){
			pthread_cond_wait(&log_cv_cs, &log_mutex); //wait until log signal not empty
		}


		char* scMessage;
		scMessage = log_buf[log_front];
		log_front = (log_front+1) % MAX_LOG_QUEUE;
		log_count--;

		FILE* p = fopen("log.txt", "a"); //append
		if(!p){
			printf("Error opening log.txt");
			exit(EXIT_FAILURE);
		}

		fprintf(p, "%s", scMessage); //print to log

		fclose(p);
		pthread_mutex_unlock(&log_mutex);
		pthread_cond_signal(&log_cv_pd); //signal queue not full
	}
}

int spellCheck(char* currentWord){
	
	for(int i = 0; i < wordCount-1; i++){
		if (strcmp(currentWord, dictionary[i]) == 0){
			return 1; //true word found
		}
	}
	//printf("Not found: %s", currentWord);
	return 0; //false, word not found
}

