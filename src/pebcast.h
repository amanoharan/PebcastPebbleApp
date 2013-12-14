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
typedef void(*PebcastPartialMessagePartStartHandler)(int thisPart, int totalParts);
typedef void(*PebcastPartialMessagePartCompleteHandler)(int thisPart, int totalParts);

typedef struct {
        PebcastMessageRequestFailedHandler onFailure;
		PebcastPartialMessageReceivedHandler onPartialMessage;
	PebcastPartialMessagePartStartHandler onPartialMessagePartStart;
	PebcastPartialMessagePartCompleteHandler onPartialMessagePartEnd;
	
} PebcastCallbacks;

bool pebcast_register_callbacks(PebcastCallbacks callbacks, void* context);


	
#endif