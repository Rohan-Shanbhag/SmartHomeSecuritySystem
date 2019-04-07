#include <stdio.h>
#include <stdlib.h>
#include "address_map_arm.h"
#include "GSInterface.h"
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>
#include <windows.h>


/* Default - 10K max memory for our param strings */
#define MAX_TWILIO_MESSAGE_SIZE 10000
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

volatile int lookUpTable[16] = {0x3F, 0x37, 0x71, 0x77, 0x38, 0x79, 0x50, 0x78}; //hex disp O, N, F, A, L, E, R, T
volatile unsigned char *(HEX3_HEX0_BASE_ptr) = (unsigned char *)HEX3_HEX0_BASE; //first 4
volatile unsigned char *(HEX5_HEX4_BASE_ptr) = (unsigned char *)HEX5_HEX4_BASE; //next 2


unsigned char x[2]; //two bytes (char is one byte, size 2)


volatile int *buttons = (int*)KEY_BASE;
int armed = 0;
bool alert = false;

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
    *((char*)HEX3_HEX0_BASE) = lookupTable[2];
    *((char*)HEX3_HEX0_BASE + 1) = lookupTable[2];
    *((char*)HEX3_HEX0_BASE + 2) = lookupTable[0];
    
    if (*buttons == 0b0001) {
        armed =1;
        // display "ARMED/ON"
        *((char*)HEX3_HEX0_BASE) = lookupTable[0];
        *((char*)HEX3_HEX0_BASE + 1) = lookupTable[1];
    }
    
    if (ReadGSRegister(GS_DEVID) == 0xE5)
    {
        GSInit();
        while (armed){
            
            MultiReadGS(GS_DATAX0, x, 2);
            signed short xInt = ((x[1] << 8) | x[0]);
            int threshold = ((xint + 100) / 25); // change to find threshold value
            
            if (threshold > 0 && threshold < 10) {
                alert = true;
                //display "Intrusion Alert" and send text
                *((char*)HEX3_HEX0_BASE) = lookupTable[7];
                *((char*)HEX3_HEX0_BASE + 1) = lookupTable[6];
                *((char*)HEX3_HEX0_BASE + 2) = lookupTable[5];
                *((char*)HEX3_HEX0_BASE + 3) = lookupTable[4];
                *((char*)HEX5_HEX4_BASE) = lookupTable[3];
                
                
                // Send twilio message
                sendSMS();
                
                Sleep(10000);
            }
            
            if (*buttons == 0b0010 && alert) {
                // change display to Armed/ON
                *((char*)HEX3_HEX0_BASE) = lookupTable[1];
                *((char*)HEX3_HEX0_BASE + 1) = lookupTable[0];
            }
            
            if (*buttons == 0b0001) {
                armed = 0;
                // display "DISARMED/OFF"
                *((char*)HEX3_HEX0_BASE) = lookupTable[2];
                *((char*)HEX3_HEX0_BASE + 1) = lookupTable[2];
                *((char*)HEX3_HEX0_BASE + 2) = lookupTable[0];
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
    char *to_number = "+6478087047";
    
    twilio_send_message(account_sid,
                        auth_token,
                        message,
                        from_number,
                        to_number,
                        NULL,
                        false);
}


/*
 * _twilio_null_write is a portable way to ignore the response from
 * curl_easy_perform
 */
size_t _twilio_null_write(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    return size * nmemb;
}

/*
 * twilio_send_message gathers the necessary parameters for a Twilio SMS or MMS.
 *
 * Inputs:
 *         - account_sid: Account SID from the Twilio console.
 *         - auth_token: Authorization from the Twilio console.
 *         - to_number: Where to send the MMS or SMS
 *         - from_number: Number in your Twilio account to use as a sender.
 *         - message_body: (Max: 1600 characters) The body of the MMS or SMS
 *               message which will be sent to the to_number.
 *         - verbose: Whether to print all the responses
 *
 *  Optional:
 *         - picture_url: If picture URL is not NULL and is a valid image url, a
 MMS will be sent by Twilio.
 */
int twilio_send_message(char *account_sid,
                        char *auth_token,
                        char *message,
                        char *from_number,
                        char *to_number,
                        char *picture_url,
                        bool verbose)
{
    
    // See: https://www.twilio.com/docs/api/rest/sending-messages for
    // information on Twilio body size limits.
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
