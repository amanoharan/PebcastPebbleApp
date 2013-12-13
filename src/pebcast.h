#ifndef PEBCAST_H
#define PEBCAST_H

bool pebcast_register();
void pebcast_init();
void pebcast_poll();
int pebcast_strtoi(char* c);

//Pebcast callbacks
typedef void(*PebcastMessageReceivedHandler)(char* msg);
typedef void(*PebcastMessageRequestFailedHandler)(char* msg);
typedef void(*PebcastPartialMessageReceivedHandler)(int thisPart, int totalParts, int key, char* msg);

typedef struct {
        PebcastMessageRequestFailedHandler onFailure;
		PebcastPartialMessageReceivedHandler onPartialMessage;
	
} PebcastCallbacks;

bool pebcast_register_callbacks(PebcastCallbacks callbacks, void* context);


	
#endif