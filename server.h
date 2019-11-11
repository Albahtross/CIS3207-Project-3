
//Includes
#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> 
#include <unistd.h>

//Queue constants
#define BUF_LEN 512
#define NUM_WORKERS 2
#define MAX_JOB_QUEUE 50 
#define MAX_LOG_QUEUE 50

//Server Globals
#define DEFAULT_PORT 8888 
#define DEFAULT_DICT "words.txt"
#define MAX_WORDS 200000
#define MAX_WORD_SIZE 58 // Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch
#define MESSAGE_SIZE 58+10 //maxwordsize + 10 ("INCORRECT\n")


int open_listenfd(int);
void* workFunc(void* args);
void* logFunc(void* args);
int spellCheck(char* currentWord);
#endif
