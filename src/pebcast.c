#include "pebble_os.h"
#include "http.h"
#include "pebcast.h"
	
void http_success(int32_t request_id, int http_status, DictionaryIterator* received, void* context);
void http_failure(int32_t request_id, int http_status, void* context);
void reconnect(void* context);
void location(float latitude, float longitude, float altitude, float accuracy, void* context);
bool pebcast_register_callbacks(PebcastCallbacks callbacks, void* context);
void recursivePollPebCast(int part);
void httpebble_error(int error_code);
int strtoi(char* c);

// POST variables
#define PAGINATION_KEY_PART 1
#define WEATHER_KEY_LATITUDE 2
#define WEATHER_KEY_LONGITUDE 3
#define PAGINATION_KEY_MAX_BYTES 4
#define PAGINATION_VALUE_MAX_BYTES 80
	
#define HTTP_COOKIE 4887
#define WEATHER_HTTP_COOKIE 1949327671

	
static PebcastCallbacks pebcast_callbacks;

static char msgThisPart[3];
static char msgTotalPartsCount[3];

static int pc_latitude, pc_longitude;
static bool pc_located;	

bool pebcast_register_callbacks(PebcastCallbacks callbacks, void* context) {
	pebcast_callbacks = callbacks;
	http_set_app_id(76782702);
 
  	return http_register_callbacks((HTTPCallbacks) {
    .success = http_success,
    .failure = http_failure,
	.reconnect=reconnect,
	.location=location	
  }, NULL);
}

void pebcast_init() {
	pc_located = false;
	http_location_request();
}

void pebcast_poll() {
	
	recursivePollPebCast(1); 
}

void reconnect(void* context) {
        recursivePollPebCast(1);
}


void location(float latitude, float longitude, float altitude, float accuracy, void* context) {
        // Fix the floats
	//pebcast_callbacks.onPartialMessage("got location");
        pc_latitude = latitude * 10000;
        pc_longitude = longitude * 10000;
        pc_located = true;
        recursivePollPebCast(1);
}


void http_success(int32_t request_id, int http_status, DictionaryIterator* received, void* context) {
  int thisPartNum=0, totalPartsCountNum=0;	
  if (request_id != HTTP_COOKIE) {
    return;
  }
//    call_count++;
	Tuple *tuple = dict_read_first(received);
	bool startNotified = false;
	
	while (tuple) {
		if(tuple->key > 0 && tuple->key < 100) {
			strncpy(msgThisPart, tuple->value->cstring, 2);
			strncpy(msgTotalPartsCount, tuple->value->cstring+2,2);
			thisPartNum = pebcast_strtoi(msgThisPart);
			totalPartsCountNum = pebcast_strtoi(msgTotalPartsCount);
			if(!startNotified) {
				startNotified = true;
				pebcast_callbacks.onPartialMessagePartStart(thisPartNum, totalPartsCountNum);
			}
				
			pebcast_callbacks.onPartialMessage(thisPartNum, totalPartsCountNum,tuple->key, tuple->value->cstring+4);
		}
		tuple = dict_read_next(received);
	}
	pebcast_callbacks.onPartialMessagePartEnd(thisPartNum, totalPartsCountNum);
	
    if(thisPartNum < totalPartsCountNum && thisPartNum > 0)
	  recursivePollPebCast(thisPartNum+1);
//	else
//	  pebcast_callbacks.onPartialMessage(msgThisPart);
}

void http_failure(int32_t request_id, int http_status, void* context) {
  httpebble_error(http_status >= 1000 ? http_status - 1000 : http_status);
}

void recursivePollPebCast(int part) {
	
	if(!pc_located) {
                http_location_request();
                return;
        }
	
  DictionaryIterator* dict;

  HTTPResult  result = http_out_get("http://pebcast.com/poll", HTTP_COOKIE, &dict);
  if(part < 1)
	  part = 1;
  dict_write_int8(dict, PAGINATION_KEY_PART, part);
  dict_write_int32(dict, WEATHER_KEY_LATITUDE, pc_latitude);
  dict_write_int32(dict, WEATHER_KEY_LONGITUDE, pc_longitude);
  dict_write_int32(dict, PAGINATION_KEY_MAX_BYTES, PAGINATION_VALUE_MAX_BYTES);
  
  if (result != HTTP_OK) {
    httpebble_error(result);
    return;
  }
  
  result = http_out_send();
  if (result != HTTP_OK) {
    httpebble_error(result);
    return;
  }
}


void httpebble_error(int error_code) {
  static char error_message[50];
	memset(error_message, 0, 50);
  clock_copy_time_string(error_message,sizeof(error_message));
  switch (error_code) {
    case HTTP_SEND_TIMEOUT:
      strcat(error_message, " Err: Send timeout");
    break;
    case HTTP_SEND_REJECTED:
      strcat(error_message, " Err: Send rejected");
    break;
    case HTTP_NOT_CONNECTED:
      strcat(error_message, " Err: Not connected");
    break;
    case HTTP_BRIDGE_NOT_RUNNING:
      strcat(error_message, " Err: Bridge not running");
    break;
    case HTTP_INVALID_ARGS:
      strcat(error_message, " Err: Invalid args");
    break;
    case HTTP_BUSY:
      strcat(error_message, " Err: HTTP Busy");
    break;
    case HTTP_BUFFER_OVERFLOW:
      strcat(error_message, " Err: Buffer overflow");
    break;
    case HTTP_ALREADY_RELEASED:
      strcat(error_message, " Err: HTTP already released");
    break;
    case HTTP_CALLBACK_ALREADY_REGISTERED:
      strcat(error_message, " Err: Callback already registered");
    break;
    case HTTP_CALLBACK_NOT_REGISTERED:
      strcat(error_message, " Err: Callback not registered");
    break;
    case HTTP_NOT_ENOUGH_STORAGE:
      strcat(error_message, " Err: Not enough storage");
    break;
    case HTTP_INVALID_DICT_ARGS:
      strcat(error_message, " Err: Invalid dictionary args");
    break;
    case HTTP_INTERNAL_INCONSISTENCY:
      strcat(error_message, " Err: Internal inconsistency");
    break;
    case HTTP_INVALID_BRIDGE_RESPONSE:
      strcat(error_message, " Err: Invalid bridge response");
    break;
    default: {
      strcat(error_message, " Err: Unknown error");
    }
	  

  }
 
  pebcast_callbacks.onFailure(error_message);
}

int pebcast_strtoi(char* c) {
	int value=0;
	int digit=0;
	while(*c) {
		digit = *c - '0';
		value *=10;
		value += digit;	
		c++;
	}
	return value;
}