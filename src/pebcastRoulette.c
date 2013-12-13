#include "pebble_os.h"
#include "http.h"
#include "pebcast.h"
#include "pebcastRoulette.h"

#define MAX_MESSAGES 6

typedef struct {
	char msgType;
	int messageDigest;
	bool vibratePreference;
	int displayedOnTopCount;
	int vibratedCount;
	int shortCode;
	int textStartOffset;
	int textEndOffset;
} PebcastRouletteMessageDigest;

void roulette_failure(char* msg);
void roulette_partial_message(int thisPart, int totalParts, int key, char* msg); 
void clearMessageDigests(PebcastRouletteMessageDigest* mdArray, int index);

static char user_message[600];
static char cast_message[1000];
static char text_message[1000];
static char fail_message[50];
#if (DEBUG > 0)
static char debug_message[50];
#endif
	
static int pcr_failureCount = 0; 
static bool vibrate = false;
static bool messagesRetrievedForFirstTime = false;
static bool locked = false;
static PebcastRouletteCallbacks roulette_callbacks;
static PebcastRouletteMessageDigest messageDigestsLocal[MAX_MESSAGES];
static PebcastRouletteMessageDigest messageDigestsTemp[MAX_MESSAGES];
static int nextMessagePointer = 0;
static PebcastRouletteDisplayType displayType = PC_ROULETTE_ALL_MESSAGES_SORTED;
static int elapsedMinsWithoutGettingValidServerResponse = 0;
	
void pebcast_roulette_init(PebcastRouletteDisplayType dispType) {	
	pebcast_register_callbacks((PebcastCallbacks) {
    	.onFailure = roulette_failure,
		.onPartialMessage = roulette_partial_message 
    }, NULL);
	displayType = dispType;
	memset(user_message,0, 600);
	memset(cast_message,0, 1000);
	memset(text_message,0, 1000);
	strcat(text_message,"Preparing to cast..");
	for(int i=0;i<MAX_MESSAGES;i++)
		clearMessageDigests(messageDigestsLocal, i);
	pebcast_init(); 
}

bool pebcast_roulette_register_callbacks(PebcastRouletteCallbacks callbacks) {
   roulette_callbacks = callbacks;
	return true;
}

void pebcast_roulette_poll() {
    pebcast_poll(); 
}

void copyToText(int index) {
	int startOff = messageDigestsLocal[index].textStartOffset;
	int endOff = messageDigestsLocal[index].textEndOffset;
	
	#if (DEBUG > 0)
		memset(debug_message,0, 50);
		snprintf(debug_message, sizeof(debug_message), "i: %d s: %d e: %d d: %d ", 
			 index, startOff, endOff, messageDigestsLocal[index].messageDigest);
		strcat(text_message, debug_message);
	#endif
	
	if(messageDigestsLocal[index].msgType == 'c') {
		strncat(text_message, cast_message +startOff, endOff - startOff);
		strncat(text_message," | ",3);
	} else if(messageDigestsLocal[index].msgType == 'u') {
		strncat(text_message, user_message +startOff, endOff - startOff);
		strncat(text_message," | ",3);
	} else if(messageDigestsLocal[index].msgType == 'f') {
		strcat(text_message, fail_message);
	} else {
		strncat(text_message, "msg type unknown", 16);
	}
}

char* pebcast_roulette_nextMessage() {
	if(locked) {// return old message itself
		if(strlen(text_message) < 1000)
			strncat(text_message, ".", 1);
		return text_message;
	}
	
	//Take the pointer to next available message, or back to where we started
	for(int i=0;i< MAX_MESSAGES;i++) {
		if(messageDigestsLocal[nextMessagePointer].textEndOffset==0)
			nextMessagePointer = (nextMessagePointer + 1 ) % MAX_MESSAGES;
		else
			break;
	}
	
	if(messageDigestsLocal[nextMessagePointer].textEndOffset==0) {
		// still nothing, not done fetching yet

		if(elapsedMinsWithoutGettingValidServerResponse > 2) {
			strcpy(text_message, "Waited for 3+ mins still no valid response from Pebcast Server. Please relaunch watch app, and report error.");
		} else {
			if(strlen(text_message) < 2)
				strcat(text_message,"Preparing to cast..");
	
			if(strlen(text_message) < 1000)
				strncat(text_message, ".", 1);		
		}
		#if (DEBUG > 0)
			snprintf(text_message, sizeof(text_message), "el: %d i: %d : f %d l: %s ft: %s ", elapsedMinsWithoutGettingValidServerResponse,
			 	nextMessagePointer, pcr_failureCount, locked ? "y" : "n", messagesRetrievedForFirstTime ? "y" : "n");
		#endif
	
		elapsedMinsWithoutGettingValidServerResponse++;
		
		return text_message;
	}
		
	memset(text_message,0, 1000);
	
	if(displayType == PC_ROULETTE_ALL_MESSAGES_SORTED) {
		int failMsgPointer = -1;
		for(int localIndex = nextMessagePointer;localIndex < nextMessagePointer + MAX_MESSAGES;localIndex++) {
			int modIndex = localIndex % MAX_MESSAGES;
			if(messageDigestsLocal[modIndex].textEndOffset==0)
				continue;
			else if(messageDigestsLocal[modIndex].msgType == 'f')
				failMsgPointer = modIndex;
			else
				copyToText(modIndex);
		}
		if(failMsgPointer >=0)
			copyToText(failMsgPointer);
	} else if(displayType == PC_ROULETTE_SINGLE_MESSAGE) {
		copyToText(nextMessagePointer);
	} else {
		//unknown message type
		strncat(text_message, "Something went wrong, please report...", 38);
	}
	
	if(messageDigestsLocal[nextMessagePointer].vibratePreference) {
		vibrate = true;
		messageDigestsLocal[nextMessagePointer].vibratedCount++;
		messageDigestsLocal[nextMessagePointer].vibratePreference = false;
	}
	
	messageDigestsLocal[nextMessagePointer].displayedOnTopCount++;
	nextMessagePointer = (nextMessagePointer + 1 ) % MAX_MESSAGES;
	return text_message;
}

bool pebcast_roulette_should_vibe() {
   bool temp = vibrate;
   vibrate = false;	
   return temp;
}

int pebcast_roulette_failureCount() {
   return pcr_failureCount;
}

void notify_failure_message(char* msg) {
	roulette_callbacks.onError(pcr_failureCount, msg);
}

void roulette_failure(char* msg) {
	pcr_failureCount++;
	memset(fail_message,0, 50);
	strcat(fail_message, msg);
	
	//add this to the roulette
	messageDigestsLocal[MAX_MESSAGES -1].msgType = 'f';
	messageDigestsLocal[MAX_MESSAGES -1].messageDigest = -100;
	messageDigestsLocal[MAX_MESSAGES -1].vibratePreference = false;
	messageDigestsLocal[MAX_MESSAGES -1].shortCode = 0;
	messageDigestsLocal[MAX_MESSAGES -1].textStartOffset = 0;
	messageDigestsLocal[MAX_MESSAGES -1].textEndOffset = strlen(fail_message) -1;
	messageDigestsLocal[MAX_MESSAGES -1].displayedOnTopCount = 0;
	messageDigestsLocal[MAX_MESSAGES -1].vibratedCount = 0;


	
	locked = false;
	notify_failure_message(msg);
	if(!messagesRetrievedForFirstTime) {
	   messagesRetrievedForFirstTime = true;
//		roulette_callbacks.onInitComplete();
	}
}

void clearMessageDigests(PebcastRouletteMessageDigest* mdArray, int index) {
	mdArray[index].msgType = 0;
	mdArray[index].messageDigest = 0;
	mdArray[index].vibratePreference = false;
	mdArray[index].displayedOnTopCount = 0;
	mdArray[index].vibratedCount = 0;
	mdArray[index].shortCode = 0;
	mdArray[index].textStartOffset = 0;
	mdArray[index].textEndOffset = 0;
}

int parseMessage(char* message, int msgSize, int digestStartIndex, char msgType) {
	int index = digestStartIndex;
	messageDigestsTemp[index].msgType = msgType;
	messageDigestsTemp[index].messageDigest = 0;
	
	bool isMsgText=false;
	bool isOption = false;
	bool isShortCode = false;
	bool isValue = false;
	bool gracefulEnd = false;	
	for (int i = 0; message[i] != 0; i++){
		//first process the tokens
		
		if(message[i] == '{') {
			isMsgText = true;
			gracefulEnd = false;
			messageDigestsTemp[index].msgType = msgType;
			messageDigestsTemp[index].textStartOffset = i+1;
			continue;//move pointer to message body
		} else if(message[i] == '}') {
			//message ended
			gracefulEnd = true;
			if(isMsgText)
				messageDigestsTemp[index].textEndOffset = i;
			
			isMsgText = false;
			isOption = false;
			isShortCode = false;
			isValue = false;
			index++;
			
			continue;//move pointer to next message
		} else if(message[i] == '|' && i+1 < msgSize && message[i+1] == '|') {
			if(isMsgText) {
				isMsgText = false;
				messageDigestsTemp[index].textEndOffset = i;
				if(i+2 < msgSize && message[i+2] != '|' && message[i+2] != ',') {
					isOption = true;
					i++; //jump one char, since delimiter is 2 chars
					continue;
				} else if(i+2 < msgSize && message[i+2] == ',') {
					isShortCode = true;
					i+= 2; //skip one more delimiter and comma
					continue;				
				} else if(i+2 < msgSize && message[i+2] == '|') {
					//no options found, keep parsing
					i++; //jump one char, since delimiter is 2 chars
					continue;
				} else {
					//possibly truncated message
					i++; //jump one char, since delimiter is 2 chars
					continue;
				}
			} else if(isOption || isShortCode) {
				isOption = false;
				isShortCode = false;
				isValue = true;
				i++; //jump one char, since delimiter is 2 chars
				continue;
			} else if(isValue) {
				//bad, this delimiter should not appear after value, just move along.
				i++;
				continue;
			}
		} 
		
		//then, process the message bytes
		
		if(isMsgText) {
			messageDigestsTemp[index].messageDigest += message[i];
		} else if(isOption) {
			if(message[i] == 'v') {
				messageDigestsTemp[index].vibratePreference = true;
			} 
		} else if(isShortCode) {
			//TODO- handle short code logic
		}
		
	}
	if(!gracefulEnd) {
		if(isMsgText)
			messageDigestsTemp[index].textEndOffset = msgSize;
		index++;
	}
	return index;
}

void sortMessages(PebcastRouletteMessageDigest* mdArray) {
	int iWeight = 0;
	int jWeight = 0;
	for(int i=0;i< MAX_MESSAGES;i++) {
		iWeight = 0;
		if(mdArray[i].textEndOffset ==0)
			iWeight -=1000;
		if(mdArray[i].vibratePreference)
			iWeight += 200;
		else
			iWeight += 100;
		if(mdArray[i].msgType == 'u')
			iWeight += 20;
		else if(mdArray[i].msgType == 'c')
			iWeight += 10;
		else	
			iWeight += 1;
		if(mdArray[i].displayedOnTopCount >0)
			iWeight -= mdArray[i].displayedOnTopCount;
		
		for(int j=i+1;j< MAX_MESSAGES;j++) {
			jWeight = 0;
			if(mdArray[j].textEndOffset ==0)
				jWeight -=1000;
			if(mdArray[j].vibratePreference)
				jWeight += 200;
			else
				jWeight += 100;
			if(mdArray[j].msgType == 'u')
				jWeight += 20;
			else if(mdArray[j].msgType == 'c')
				jWeight += 10;
			else	
				jWeight += 1;
			if(mdArray[j].displayedOnTopCount >0)
				jWeight -= mdArray[j].displayedOnTopCount;
			if(iWeight < jWeight) {
				//swap
				PebcastRouletteMessageDigest temp = mdArray[i];
				mdArray[i] = mdArray[j];
				mdArray[j] = temp;
			}
		}
	}
}



void mergeToMessageDigestsLocalAndSort() {
	int firstFreeSlot = MAX_MESSAGES;
	for(int i=0;i< MAX_MESSAGES;i++) {
		if(messageDigestsLocal[i].messageDigest == 0) {
			//empty slot, skip
			if(firstFreeSlot > i)
				firstFreeSlot = i;
		} else {
			bool matched = false;
			for(int j=0;j< MAX_MESSAGES;j++) {
				if(messageDigestsLocal[i].messageDigest == messageDigestsTemp[j].messageDigest) {
					//old message, repeating again. Merge them. Incoming message may request vibrate, but if we already vibed for that, no need to repeat
					messageDigestsLocal[i].textStartOffset =  messageDigestsTemp[j].textStartOffset;
					messageDigestsLocal[i].textEndOffset =  messageDigestsTemp[j].textEndOffset;
					matched = true;
					//clear the temp row, so that we will end up with whats new
					clearMessageDigests(messageDigestsTemp, j);
					break;
				}
			}
			if(!matched) {
				//stale message, not resent. Remove it.
				clearMessageDigests(messageDigestsLocal, i);
				if(firstFreeSlot > i)
					firstFreeSlot = i;
			}
		}
	}
	//now copy the new ones
	for(int i=0;i< MAX_MESSAGES;i++) {
		if(messageDigestsTemp[i].messageDigest != 0) {
			//new message
			messageDigestsLocal[firstFreeSlot].msgType = messageDigestsTemp[i].msgType;
			messageDigestsLocal[firstFreeSlot].messageDigest = messageDigestsTemp[i].messageDigest;
			messageDigestsLocal[firstFreeSlot].vibratePreference = messageDigestsTemp[i].vibratePreference;
			messageDigestsLocal[firstFreeSlot].shortCode = messageDigestsTemp[i].shortCode;
			messageDigestsLocal[firstFreeSlot].textStartOffset = messageDigestsTemp[i].textStartOffset;
			messageDigestsLocal[firstFreeSlot].textEndOffset = messageDigestsTemp[i].textEndOffset;
			bool matched = false;
			for(int j= firstFreeSlot; j< MAX_MESSAGES;j++) {
				if(messageDigestsLocal[j].messageDigest == 0) {
					firstFreeSlot = j;
					matched = true;
					break;
				}
			}
			if(!matched) {
				//no more slots, quit
				break;
			}
				
		}
	}
	//sort
	sortMessages(messageDigestsLocal);
}


void processMessage() {
	for(int i=0;i<MAX_MESSAGES;i++)
		clearMessageDigests(messageDigestsTemp, i);
	
	int index = parseMessage(user_message, strlen(user_message), 0,'u'); 
	parseMessage(cast_message, strlen(cast_message), index,'c'); 
	mergeToMessageDigestsLocalAndSort();
}



void roulette_partial_message(int thisPart, int totalParts, int key, char* msg) {
	if(thisPart < totalParts)
		locked = true;
	pcr_failureCount = 0;
	
	if(thisPart == 1) {
		memset(user_message,0, 600);
		memset(cast_message,0, 1000);
	}
	
	if(key == 1) { // user message aka mails
		if( strlen(user_message) + strlen(msg) < 600)
	   		strcat(user_message, msg);
		else {
			int available = 600 - (strlen(user_message) + strlen(msg) +1);
			if(available > 0)
				strncat(user_message, msg, available);
			notify_failure_message("Message too big!");
		}
		
	} else {
		if( strlen(cast_message) + strlen(msg) < 1000)
		   strcat(cast_message, msg);
		else {
			int available = 600 - (strlen(cast_message) + strlen(msg) +1);
			if(available > 0)
				strncat(cast_message, msg, available);
			notify_failure_message("Message too big!");
		}
	}
	if(thisPart == totalParts) {
		processMessage();
		nextMessagePointer = 0;
		locked = false;
		if(!messagesRetrievedForFirstTime) {
	   		messagesRetrievedForFirstTime = true;
			roulette_callbacks.onInitComplete();
	    }
	}



}