#ifndef PEBCASTROULETTE_H
#define PEBCASTROULETTE_H

#define DEBUG 0

typedef void(*PebcastRouletteInitCompleteHandler)();	
typedef void(*PebcastRouletteFailureHandler)(int failCount, char* msg);
// Shared values.
typedef enum {
        PC_ROULETTE_ALL_MESSAGES_SORTED      = 0,
        PC_ROULETTE_SINGLE_MESSAGE           = 1
} PebcastRouletteDisplayType;


typedef struct {
        PebcastRouletteInitCompleteHandler onInitComplete;
		PebcastRouletteFailureHandler onError;	
} PebcastRouletteCallbacks;



bool pebcast_roulette_register_callbacks(PebcastRouletteCallbacks callbacks);

void pebcast_roulette_init(PebcastRouletteDisplayType msgType);
void pebcast_roulette_poll();
char* pebcast_roulette_nextMessage();
bool pebcast_roulette_should_vibe();
int pebcast_roulette_failureCount();

#endif