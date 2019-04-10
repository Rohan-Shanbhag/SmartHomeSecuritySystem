/*
 Rohan Shanbhag - 250955627
 Jodhveer Gill - 250949498
 Osama Masud - 250957075
 Eshaan Tyagi - 250956760
*/

#include <stdio.h>
#include <stdlib.h>
#include "address_map_arm.h"
#include "GSInterface.h"
#include <unistd.h>
#include <curl/curl.h>




/* Default - 10K max memory for our param strings */
#define MAX_TWILIO_MESSAGE_SIZE 10000

// defines boolean operator in C as it does not exist in library
typedef enum { false, true }    bool;

/** Prototypes */
void sendSMS(void);
int twilio_send_message(char *account_sid,
                        char *auth_token,
                        char *message,
                        char *from_number,
                        char *to_number,
                        char *picture_url,
                        bool verbose);

volatile int lookupTable[] = {0x3F, 0x37, 0x71, 0x77, 0x38, 0x79, 0x50, 0x78, 0x00}; //hex disp O, N, F, A, L, E, R, T, off
volatile unsigned char *(HEX3_HEX0_BASE_ptr) = (unsigned char *)HEX3_HEX0_BASE; //first 4
volatile unsigned char *(HEX5_HEX4_BASE_ptr) = (unsigned char *)HEX5_HEX4_BASE; //next 2


unsigned char x[2]; //two bytes (char is one byte, size 2)

volatile int *switches = (int*)SW_BASE;
volatile int *buttons = (int*)KEY_BASE;
int armed = 0;
bool alert = false;
// password is read from switches
int password = 7;
int ReadSwitches(void) {
    volatile int a;
    a = *(SW_ptr) &= 0xF;
    return a;
}

void GSInit(void) {
    //for +/- 2G range set D0 and D1 to 0
    WriteGSRegister(0x31, 8);
    
    // Configure for 200Hz sampling rate.  Sampling rate of 10 == 100Hz.  Each incremement after that is a doubling.
    WriteGSRegister(0x2c, 11);
    
    // Configure to begin measurement
    WriteGSRegister(0x2d, 8);
}



int main (void) {
    
    I2C0Init();
    // initial state is OFF
    *(HEX3_HEX0_BASE_ptr) = lookupTable[2];
    *(HEX3_HEX0_BASE_ptr + 1) = lookupTable[2];
    *(HEX3_HEX0_BASE_ptr + 2) = lookupTable[0];
    
    
    // ensures reading of correct register
    if (ReadGSRegister(GS_DEVID) == 0xE5)
    {
        // initialization of GS sensor
        GSInit();
		
		while (1){
            // password must be inputed before arming system
			if (*buttons == 0b0001 && ReadSwitches() == password) {
			armed =1;
			// display "ARMED/ON"
			*(HEX3_HEX0_BASE_ptr) = lookupTable[1];
			*(HEX3_HEX0_BASE_ptr + 1) = lookupTable[0];
			*(HEX3_HEX0_BASE_ptr + 2) = lookupTable[8];
			}
			while (armed){
				
				MultiReadGS(GS_DATAY0, x, 2);
				signed short xInt = ((x[1] << 8) | x[0]);
				int threshold = ((xInt + 260) / 52); // change to find threshold value
				
				if (threshold < 2) {
					alert = true;
					//display "Intrusion Alert" and send text
					*(HEX3_HEX0_BASE_ptr) = lookupTable[7];
					*(HEX3_HEX0_BASE_ptr+ 1) = lookupTable[6];
					*(HEX3_HEX0_BASE_ptr + 2) = lookupTable[5];
					*(HEX3_HEX0_BASE_ptr + 3) = lookupTable[4];
					*(HEX5_HEX4_BASE_ptr) = lookupTable[3];
					*(HEX5_HEX4_BASE_ptr + 1) = lookupTable[8];
					
					
					// Send twilio message
					sendSMS();
				  
				}
				
				if (*buttons == 0b0010 && alert) {
					
					// change display to Armed/ON
					*(HEX3_HEX0_BASE_ptr) = lookupTable[1];
					*(HEX3_HEX0_BASE_ptr + 1) = lookupTable[0];
					*(HEX3_HEX0_BASE_ptr + 2) = lookupTable[8];
					*(HEX3_HEX0_BASE_ptr + 2) = lookupTable[8];
					
					*(HEX5_HEX4_BASE_ptr) = lookupTable[8];
					*(HEX5_HEX4_BASE_ptr + 1) = lookupTable[8];
					alert = false;
				}
				
				if (*buttons == 0b0100) {
					armed = 0;
					// display "DISARMED/OFF"
					*(HEX3_HEX0_BASE_ptr) = lookupTable[2];
					*(HEX3_HEX0_BASE_ptr + 1) = lookupTable[2];
					*(HEX3_HEX0_BASE_ptr + 2) = lookupTable[0];
					
					
					*(HEX5_HEX4_BASE_ptr) = lookupTable[8];
					*(HEX5_HEX4_BASE_ptr + 1) = lookupTable[8];
				}
				
			}
        }
        
    }
}


/** PRAGMA MARK: Twilio SMS */

/** Twilio Helper Function */
void sendSMS(void) {
    char *account_sid = "AC79e06114ec6ec43c135b256436cbc7f7";
    char *auth_token = "41b4c5557dd0b23577f055ebe708924d";
    char *message = "ALERT! Someone has intruded your home. This is an automated message. Seek help or acknowledge immediately.";
    char *from_number = "+16474931887";
    char *to_number = "+16478087047";
    
    twilio_send_message(account_sid,
                        auth_token,
                        message,
                        from_number,
                        to_number,
                        NULL,
                        false);
}


 
size_t _twilio_null_write(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    return size * nmemb;
}


int twilio_send_message(char *account_sid,
                        char *auth_token,
                        char *message,
                        char *from_number,
                        char *to_number,
                        char *picture_url,
                        bool verbose)
{
    
    
    if (strlen(message) > 1600) {
        fprintf(stderr, "SMS send failed.\n"
                "Message body must be less than 1601 characters.\n"
                "The message had %zu characters.\n", strlen(message));
        return -1;
    }
    
    CURL *curl;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    
    char url[MAX_TWILIO_MESSAGE_SIZE];
    snprintf(url,
             sizeof(url),
             "%s%s%s",
             "https://api.twilio.com/2010-04-01/Accounts/",
             account_sid,
             "/Messages");
    
    char parameters[MAX_TWILIO_MESSAGE_SIZE];
    if (!picture_url) {
        snprintf(parameters,
                 sizeof(parameters),
                 "%s%s%s%s%s%s",
                 "To=",
                 to_number,
                 "&From=",
                 from_number,
                 "&Body=",
                 message);
    } else {
        snprintf(parameters,
                 sizeof(parameters),
                 "%s%s%s%s%s%s%s%s",
                 "To=",
                 to_number,
                 "&From=",
                 from_number,
                 "&Body=",
                 message,
                 "&MediaUrl=",
                 picture_url);
    }
    
    
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, parameters);
    curl_easy_setopt(curl, CURLOPT_USERNAME, account_sid);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, auth_token);
    
    if (!verbose) {
        curl_easy_setopt(curl,
                         CURLOPT_WRITEFUNCTION,
                         _twilio_null_write);
    }
    
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    long http_code = 0;
    curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    if (res != CURLE_OK) {
        if (verbose) {
            fprintf(stderr,
                    "SMS send failed: %s.\n",
                    curl_easy_strerror(res));
        }
        return -1;
    } else if (http_code != 200 && http_code != 201) {
        if (verbose) {
            fprintf(stderr,
                    "SMS send failed, HTTP Status Code: %ld.\n",
                    http_code);
        }
        return -1;
    } else {
        if (verbose) {
            fprintf(stderr,
                    "SMS sent successfully!\n");
        }
        return 0;
    }
    
}
