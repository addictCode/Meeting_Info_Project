#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <WiFi.h>
#include <time.h>
#include <MD5.h>
#include <SPI.h>
//EPD
#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"
#include "GUI_Paint.h"
#include "fonts.h"

//use your 2.4G wifi name and password. focus: need to be 2.4G wifi, 5G wifi don't work
const char* ssid = "OpenWrt-H68K";
const char* password = "00000000";

//Define canvas space  
unsigned char BlackImage[EPD_ARRAY];

struct InfoItem{
  const char* roomName;
  const char* startTime;
  const char* endTime;
};

void setup() {

   pinMode(A5, INPUT);  //BUSY
   pinMode(A0, OUTPUT); //RES 
   pinMode(A3, OUTPUT); //DC   
   pinMode(A1, OUTPUT); //CS   
   //SPI
   SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0)); 
   SPI.begin (); 

  //set serial rate
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
}

void loop() {
  //create token
  String token = getMD5();
  //get meeting info
  makeHttpPostRequest(token);
  //every 5 minutes to refresh data
  delay(300000);
}

//extract HH:mm fommat from original
const char* extractTime(const char* time) {
  // Find the position of the first colon
  const char* colonPos = strchr(time, ':');
  if (colonPos == NULL)
    return NULL; // Return NULL if no colon is found

  // Calculate the length of the time string
  int timeLength = 5; // Hour, minute, and colon

  // Allocate memory for the time string
  char* extractedTime = (char*)malloc((timeLength + 1) * sizeof(char));
  if (extractedTime == NULL)
    return NULL; // Return NULL if memory allocation fails

  // Copy the time substring into the newly allocated memory
  strncpy(extractedTime, colonPos - 2, timeLength);
  extractedTime[timeLength] = '\0'; // Null-terminate the string

  return extractedTime;
}

// compare time string
int timeCompare(const char* a, const char* b) {
  int hourA, minuteA, hourB, minuteB;
  
  // transfer hour and minute
  sscanf(a, "%d:%d", &hourA, &minuteA);
  sscanf(b, "%d:%d", &hourB, &minuteB);

  // calculate total
  int totalMinutesA = hourA * 60 + minuteA;
  int totalMinutesB = hourB * 60 + minuteB;

  // compare
  return totalMinutesA - totalMinutesB;
}

// insert sort algrithem
void sortInfoItems(InfoItem* items, size_t itemCount) {
  for (size_t i = 1; i < itemCount; ++i) {
    InfoItem key = items[i];
    int j = i - 1;
    while (j >= 0 && timeCompare(items[j].startTime, key.startTime) > 0) {
      items[j + 1] = items[j];
      j = j - 1;
    }
    items[j + 1] = key;
  }
}

//to get a token that need to encrypt a data
String getMD5(){

  // get current time
  configTime(0, 0, "pool.ntp.org");
  while (time(nullptr) < 100000) {
    delay(1000);
    Serial.println("Waiting for time sync...");
  }
  time_t now = time(nullptr);
  struct tm *timeinfo;
  timeinfo = localtime(&now);

  // set format "YYYYMMDD"
  char formattedDate[9];
  strftime(formattedDate, sizeof(formattedDate), "%Y%m%d", timeinfo);

  // splice string
  char resultString[30]; // setup enough buffer
  sprintf(resultString, "HASed@24#%s", formattedDate);

  //MD5 encrypt
  unsigned char* hash=MD5::make_hash(resultString);
  //generate the digest (hex encoding) of our hash
  char* md5str = MD5::make_digest(hash, 16);
  String token = String(md5str);
  free(hash);

  //Give the Memory back to the System if you run the md5 Hash generation in a loop
  free(md5str);
  return token;
}

//make https request
void makeHttpPostRequest(String token) {

  // Define the meeting name
  String IAG_small_meeting_room = "IAG小会议室";
  String IAG_big_meeting_room = "IAG大会议室";

  HTTPClient http;
  JSONVar myObject1;
  JSONVar myObject2;
  JSONVar bookList;

  int httpResponseCode1;
  int httpResponseCode2;

  InfoItem infoItem1[10];
  InfoItem infoItem2[10];
  int index1 = 0;
  int index2 = 0;

  // Specify the target URL
  String url = "http://k3cloud.seeed.cc:8098/api/HA/GetMeetingBookInfo";
  
//-----------------------------------------http1-----------------------------------------
  // Start the HTTP POST request
  http.begin(url);

  // Set the content type header
  http.addHeader("Content-Type", "application/json");

  // Define the payload with parameters
  String jsonPayload1 = "{\"Token\":\""+ token +"\",\"MeetingName\":\"" + IAG_small_meeting_room + "\"}";

  // Set the content length header
  http.addHeader("Content-Length", String(jsonPayload1.length()));

  // Send the POST request
  httpResponseCode1 = http.POST(jsonPayload1);

  // Check for a successful response
  if (httpResponseCode1 > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode1);

    // You can handle the response here if needed
    String response1 = http.getString();
    Serial.println(response1);

    // Parsing JSON data
    myObject1 = JSON.parse(response1);
    if (JSON.typeof(myObject1) == "undefined") { // check if it works
      Serial.println("Parsing failed");
      return;
    }

    //extract data to infoItem1
    JSONVar tempList1 = myObject1["data"];
    for(int i=0;i<tempList1.length();i++){
      JSONVar item = tempList1[i];
      infoItem1[index1].roomName = "Small Meeting Room";
      const char* dateTimeString1 = item["StartTime"];
      infoItem1[index1].startTime = extractTime(dateTimeString1);
      const char* dateTimeString2 = item["EndTime"];
      infoItem1[index1].endTime = extractTime(dateTimeString2);
      index1++;
    }
    //sort the infoItem1
    sortInfoItems(infoItem1, index1);
  } else {
    Serial.print("HTTP Request failed. Error code: ");
    Serial.println(httpResponseCode1);
  }
  
  // Close the connection
  http.end();
//-----------------------------------------http2-----------------------------------------

  // Start the second HTTP POST request
  http.begin(url);

  // Set the content type header
  http.addHeader("Content-Type", "application/json");

  // Define the payload with parameters
  String jsonPayload2 = "{\"Token\":\""+ token +"\",\"MeetingName\":\"" + IAG_big_meeting_room + "\"}";

  // Set the content length header
  http.addHeader("Content-Length", String(jsonPayload2.length()));

  // Send the POST request
  httpResponseCode2 = http.POST(jsonPayload2);

  // Check for a successful response
  if (httpResponseCode2 > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode2);

    // You can handle the response here if needed
    String response2 = http.getString();
    Serial.println(response2);

    // Parsing JSON data
    myObject2 = JSON.parse(response2);
    if (JSON.typeof(myObject2) == "undefined") { // check if it works
      Serial.println("Parsing failed");
      return;
    }
    JSONVar tempList2 = myObject2["data"];
      for(int i=0;i<tempList2.length();i++){
        JSONVar item = tempList2[i];
        infoItem2[index2].roomName = "Big Meeting Room";
        const char* dateTimeString1 = item["StartTime"];
        infoItem2[index2].startTime = extractTime(dateTimeString1);
        const char* dateTimeString2 = item["EndTime"];
        infoItem2[index2].endTime = extractTime(dateTimeString2);
        index2++;
      }
      //sort the infoItem2
      sortInfoItems(infoItem2, index2);

  } else {
    Serial.print("HTTP Request failed. Error code: ");
    Serial.println(httpResponseCode2);
  }

  // Close the connection
  http.end();

  //show meeting info
  //only normal http response then change the screen
  if(httpResponseCode1==200){
    Paint_NewImage(BlackImage, EPD_WIDTH, EPD_HEIGHT, 180, WHITE); //Set canvas parameters, GUI image rotation, please change 0 to 0/90/180/270.
    Paint_SelectImage(BlackImage); //Select current settings.
    EPD_Init(); //Full screen refresh initialization.
    Paint_Clear(WHITE); //Clear canvas.
    Paint_DrawString_EN(280, 20, "Meeting Info List", &Font24, WHITE, BLACK); //17*24.
    Paint_DrawLine(50, 50, 750, 50, BLACK, LINE_STYLE_SOLID, DOT_PIXEL_1X1);
    Paint_DrawLine(50, 90, 750, 90, BLACK, LINE_STYLE_SOLID, DOT_PIXEL_1X1);
    Paint_DrawString_EN(50, 60, "Small Meeting Room", &Font24, WHITE, BLACK); //17*24.
    Paint_DrawString_EN(450, 60, "Big Meeting Room", &Font24, WHITE, BLACK); //17*24.

    for(int i=0;i<index1;i++){
      Paint_DrawString_EN(50, 100+(i*30), infoItem1[i].startTime, &Font24, WHITE, BLACK); //17*24.
      Paint_DrawString_EN(150, 100+(i*30), "-", &Font24, WHITE, BLACK); //17*24.
      Paint_DrawString_EN(190, 100+(i*30), infoItem1[i].endTime, &Font24, WHITE, BLACK); //17*24.
    }
    for(int i=0;i<index2;i++){
      Paint_DrawString_EN(450, 100+(i*30), infoItem2[i].startTime, &Font24, WHITE, BLACK); //17*24.
      Paint_DrawString_EN(550, 100+(i*30), "-", &Font24, WHITE, BLACK); //17*24.
      Paint_DrawString_EN(590, 100+(i*30), infoItem2[i].endTime, &Font24, WHITE, BLACK); //17*24.
    }

    EPD_Display(BlackImage);//Display GUI image.
    EPD_DeepSleep(); //EPD_DeepSleep,Sleep instruction is necessary, please do not delete!!!
    delay(2000); //Delay for 2s.
  }

  //to free memory
  for (int i = 0; i < index1; i++) {
      free(infoItem1[i].startTime);
      free(infoItem1[i].endTime);
      infoItem1[i].startTime = NULL;
      infoItem1[i].endTime = NULL;
  }
  for (int i = 0; i < index2; i++) {
      free(infoItem2[i].startTime);
      free(infoItem2[i].endTime);
      infoItem1[i].startTime = NULL;
      infoItem1[i].endTime = NULL;
  }
}



