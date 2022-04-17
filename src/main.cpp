// Smart Led V2:- Access the user data from the htpps server of hamropalika and display it on the Display.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PxMatrix.h>
#include "OneButton.h"               //we need the OneButton library to detect diffrent push functions.
#include <Fonts/FreeSansBold9pt7b.h> //Custom Font for Name.
#include <Adafruit_I2CDevice.h>

#include <Preferences.h> //to store the values to flash memory
Preferences storage;     // create a instance for the code

void update_data(void); // function declaration
void doubleclick();
void doubleclick1(uint8_t ofset);
void scrolling_notice(String Snotice);
/*******************************************************************************************/

// Pins for LED MATRIX
#define P_LAT 16 // 22
#define P_A 19
#define P_B 2 // 23
#define P_C 18
#define P_D 4 // 5
#define P_E 15
#define P_OE 5

const int btn = 26; // Button (Pulled High);
const int ldr = 27; // Light Sensor(LDR) (AnalogInput)

OneButton button(btn, true); //[Active LOW]   //attach a button on pin 4 to the library

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define matrix_width 128
#define matrix_height 64
/* This defines the 'on' time of the display is us. The larger this number,
the brighter the display. If too large the ESP will crash */

uint8_t display_draw_time = 50; // 30-70 is usually fine
PxMATRIX display(128, 64, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);

// Wifi credincials
const char *ssid = "NC_LINK_Wireless"; // smarttechnepal
const char *password = "66666666";     // stn1234#!@123

const char *Hostname = "helpdesk.hamropalika.org";
String get_URI = "/api/v2/employeeByDeviceId/2";  // http GET API
String post_URI = "/api/v2/updateEmployeeStatus"; // Post API
// const String HTTP_METHOD = "POST"; // or "GET"

String Presence_status = "onfield"; // Absent/Present/onfield(non case sensative)
String Device_ID = "2";             // Unique Device ID
String queryString = String("?attendances=") + String(Presence_status) + String("&deviceId=") + String(Device_ID);

void parseHTTP_data(void);
const uint16_t port = 443;

// For HTTPS requests
WiFiClientSecure https;


long httpget_timer;
/******************************************************************************************************/
int statas = 1;  // to chage the presence staus;
int start_time;  // to periodically change contacts.
int start_time1; // to update the email
int expos = 5;   // X-pos of email
int current_expos = expos;
int mailcharcount = 1;        // for scrolling Email.
int scrollinterval = 100;     //[Inverse]speed at which the texts scrolls.
int httpget_interval = 15000; // 15 sec interval

/******************************************************************************************************/
int start_time2;    // for scrolling notice
int noticexpos = 5; // for scrolling notice
int current_noticexpos = noticexpos;
int noticecharcount = 1;    // for scrolling notice
const int interval = 10000; // Delay at which the contacts[1000= 1Second].

/******************************************************************************************************/
int lastnum = 0;
int num = 1;
int i1 = 0;
int current_statas; // TO detect the change in attendance
/******************************************************************/
// Some standard colors in RGB format
uint16_t myORANGE = display.color565(255, 102, 10);
uint16_t myRED = display.color565(255, 10, 10);
uint16_t myGREEN = display.color565(100, 255, 0);
uint16_t myBLUE = display.color565(0, 0, 255);
uint16_t myWHITE = display.color565(200, 200, 200);
uint16_t myYELLOW = display.color565(255, 170, 29);
uint16_t myCYAN = display.color565(0, 255, 255);
uint16_t myMAGENTA = display.color565(255, 0, 255);
uint16_t myBLACK = display.color565(0, 0, 0);
uint16_t myCOLORS[9] = {myRED, myGREEN, myBLUE, myWHITE, myYELLOW, myCYAN, myMAGENTA, myBLACK, myORANGE};

/******************************************************************************************************/
// Decalration of Strings that are responsible to display the info
String disp_data_firstname = "";
String disp_data_lastname = "";
String disp_data_designation = "";
String disp_data_email = "";
String disp_data_contact1 = "";
String disp_data_contact2 = "";
String disp_data_attendance = "Absent";
String disp_data_notice = "";
String Innotice = "Available!";
String Busynotice = "Engaged!";
bool disp_data_isPermanent = 0;

// to retreve the data from the web.
String data_firstname = "";   // Ashish
String data_lastname = "";    // Manandhar
String data_attendance = "";  // Absent/Present/onfield(non case sensative)
String data_phone = "";       // "234234"
String data_phone2 = "";      // not yet added in api
String data_email = "";       // "ash@gmail.com"
String data_designation = ""; // "नगर प्रमुख"
String data_outNote = "";     // "sick leave"
bool data_isPermanent = 0;    // false

/******************************************************************************************************/
// Set Color for different values
uint16_t namecolor = myYELLOW;
uint16_t designationcolor = myGREEN;
uint16_t contactcolor = myYELLOW;
uint16_t noticecolor = myYELLOW;

uint16_t statasincolor = myWHITE;
uint16_t statasbusycolor = myCYAN;
uint16_t statasoutcolor = myRED;

uint16_t bordercolor = myORANGE;
uint16_t inbusyoutcolor = myMAGENTA;

void IRAM_ATTR display_updater()
{
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}

void display_update_enable(bool is_enable)
{
  if (is_enable)
  {
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &display_updater, true);
    timerAlarmWrite(timer, 4000, true);
    timerAlarmEnable(timer);
  }
  else
  {
    timerDetachInterrupt(timer);
    timerAlarmDisable(timer);
  }
}

void wifi_connect()
{

  Serial.print("Connecting to ");
  Serial.println(ssid);

  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int wifi_timeout = 6000; // time this would take try connecting to wifi -> 10 sec
  long startwifitimer = millis();

  while ((WiFi.status() != WL_CONNECTED) || millis() - startwifitimer < wifi_timeout)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void makeHTTPRequest(String http_method, bool isPost, String data_URI)
{ //[Make HTTP get request] String http_method, String data_query_String

  // if it fails to connect with the server then print connection failed else, continue with other code
  https.setTimeout(10000);
  if (!https.connect(Hostname, port))
  {
    Serial.println(F("Connection failed"));
    return; // returns back to the main thread.
  }
  display.display(); // for the display
  yield();

  // Request the get data from the server
  Serial.println(F("Connected!"));

  // Send HTTP request
  https.print(http_method + " "); // API URL ("POST " + post_URI + queryString + " HTTP/1.0");
  https.print((String)data_URI);

  if (isPost == true)
  {
    https.print((String)queryString); // Only Used for HTTP POst
  }
  https.println(" HTTP/1.0");

  https.println("Host: " + (String)Hostname); // Server URL
  // client.println(queryString);
  https.println("Connection: close");
  https.println(F("Cache-Control: no-cache"));

  if (https.println() == 0)
  {
    Serial.println("Failed to send request");
    return;
  }
  display.display();
  // Check HTTP status
  char status[32] = {0};
  https.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.0 200 OK") != 0)
  {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    return;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!https.find(endOfHeaders))
  {
    Serial.println(F("Invalid response"));
    https.stop();
    return;
  }

  display.display();
  /* while (https.available() && https.peek() != '{')
   {
     char c = 0;
     https.readBytes(&c, 1);
     Serial.print(c);
     Serial.println("BAD");
   }
 */
  parseHTTP_data(); // call the function to retrive the data
  https.stop();
}

/*-------------------------------------------------------------------------------------------------------*/
// Parse the data available from http get
void parseHTTP_data()
{

  // Allocate the JSON document
  // StaticJsonBuffer <768> doc;
  DynamicJsonDocument doc(1024);

  // Parse JSON object
  auto err = deserializeJson(doc, https);

  if (err)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(err.c_str());
    return;
  }
  display.display();

  if (err == DeserializationError::NoMemory)
  {
    Serial.println("");
    Serial.println("Memory Ran out!!");
    Serial.println("");
    Serial.println("");
    delay(5000);
  }

  JsonObject data = doc["data"];
  int Data_id = data["id"];                       // 1
  String Data_attendance = data["attendances"];   // nullptr
  String Data_name = data["name"];                // "Ashish Manandhar"
  String Data_phone = data["phone"];              // "234234"
  String Data_phone2 = "9745";                    // not yet added
  String Data_email = data["email"];              // "ash@gmail.com"
  String Data_designation = data["designation"];  // "नगर प्रमुख"
  String Data_outNote = data["attendancesNote"];  // "sick leave"
  bool Data_isPermanent = data["bool_permanent"]; // false

  // to retreve the data from the web.
  display.display();

  data_attendance = Data_attendance; // "234234"
  if (Data_email)
  {
    data_email = Data_email; // "ash@gmail.com"
  }
  if (Data_phone)
  {
    data_phone = Data_phone; // 9814881110
  }

  data_phone2 = Data_phone2; // not yer added to api

  data_designation = Data_designation; // "नगर प्रमुख"
  data_outNote = Data_outNote;         // "sick leave"
  data_isPermanent = Data_isPermanent; // false

  // Extract the Firstname and Lastname from the main String
  int length_name = Data_name.indexOf(" ");
  data_firstname = Data_name.substring(0, length_name); // get the firstname
  data_lastname = Data_name.substring(length_name + 1); // get lastname

  update_data(); // call the function to save the data into flash
  doc.clear();   // clear ram?

  /*
  Serial.print("free heap memory: ") ;
  Serial.println(ESP.getFreeHeap());
  doc.~BasicJsonDocument();
  Serial.print("free heap memory: ");
  Serial.print(ESP.getFreeHeap());
  */
}

void update_data()
{
  display.display();
  // Read the latest data from the flash first and compare with new data received from get;
  disp_data_firstname = storage.getString("firstname", "");
  disp_data_lastname = storage.getString("lastname", "");
  disp_data_designation = storage.getString("designation", "");
  disp_data_contact1 = storage.getString("contact1", "");
  disp_data_contact2 = storage.getString("contact2", "");
  disp_data_email = storage.getString("email", "");
  disp_data_notice = storage.getString("notice", "");
  disp_data_isPermanent = storage.getBool("isPermanent", 0);
  // disp_data_attendance = storage.getString("attendance", "");
  display.display();

  Serial.println("");
  Serial.println("http data; Firstname: " + data_firstname);
  Serial.println("http data; Lastname: " + data_lastname);
  Serial.println("http data; Designation: " + data_designation);
  Serial.println("http data; contact1: " + data_phone);
  // Serial.println("http data; contact2: " + );
  Serial.println("http data; emial: " + data_email);
  Serial.println("http data; Notice: " + data_outNote);
  Serial.println("http data; attendance: " + data_attendance);
  Serial.println("");

  display.display();
  if (data_firstname != disp_data_firstname) //[FirstName]
  {
    disp_data_firstname = data_firstname;
    storage.putString("firstname", data_firstname);
    Serial.println(data_firstname + " is Written into Flash successfully");
  }

  if (data_lastname != disp_data_lastname) //[LastName]
  {
    disp_data_firstname = data_lastname;
    storage.putString("lastname", data_lastname);
    Serial.println(data_lastname + " is Written into Flash successfully");
  }

  if (data_designation != disp_data_designation) //[Designation]
  {
    disp_data_firstname = data_designation;
    storage.putString("designation", data_designation);
    Serial.println(data_designation + " is Written into Flash successfully");
  }

  if (data_phone != disp_data_contact1) //[Contact1]
  {
    disp_data_contact1 = data_phone;
    storage.putString("contact1", data_phone);
    Serial.println(data_phone + " is Written into Flash successfully");
  }
  display.display();

  if (data_phone2 != disp_data_contact2) //[Contact1]
  {
    disp_data_contact2 = data_phone2;
    storage.putString("contact2", data_phone2);
    Serial.println(data_phone2 + " is Written into Flash successfully");
  }

  if (data_email != disp_data_email) //[Email]
  {
    disp_data_email = data_email;
    storage.putString("email", data_email); // store the object into the flash on key "email"
    Serial.println(data_email + " is Written into Flash successfully");
  }

  if (data_outNote != disp_data_notice) //[Notice]
  {
    disp_data_notice = data_outNote;
    storage.putString("notice", data_outNote); // store the object into the flash on key "email"
    Serial.println(data_outNote + " is Written into Flash successfully");
  }

  /* After the data is Read from http get, compare with the memory values,
   *if they are different then identify which one it is and adust the display
   * Status accordingly.
   */

  if (data_attendance != disp_data_attendance) //[attendance]
  {
    disp_data_attendance = data_attendance;
    storage.putString("attendance", data_attendance); // Save to flash

    if (data_attendance.equalsIgnoreCase("Present"))
    {
      statas = 1; // Make the attandance "IN"
      current_statas = statas;
      display.clearDisplay();
      Serial.println("Attendance Status: " + data_attendance + " " + statas);
      doubleclick1(1); // call the function to change
    }
    if (data_attendance.equalsIgnoreCase("onfield"))
    {
      statas = 2; // Make the attandance "BUSY"
      current_statas = statas;
      display.clearDisplay();
      doubleclick1(1); // call the function to change
    }
    if (data_attendance.equalsIgnoreCase("Absent"))
    {
      statas = 3; // Make the attandance "OUT"
      current_statas = statas;
      display.clearDisplay();
      doubleclick1(1); // call the function to change
    }

    Serial.println(data_attendance + " is Written into Flash successfully");
  }

  if (data_isPermanent != disp_data_isPermanent)
  {
    disp_data_isPermanent = data_isPermanent;
    storage.putBool("isPermanent", data_isPermanent);
    Serial.println(data_attendance + " is Written into Flash successfully");
  }
} // End Function

///////////////////////////////////////////////////////////////////
//                                                               //
//                                                               //
//                                                               //
///////////////////////////////////////////////////////////////////

// presences Status, text color of status, X-position to print test.
void inbusyout(String presence, uint16_t statascolor, uint8_t sxpos) //     [In/Busy/Out]
{
  //  display.fillRoundRect(81,45,44,15, 2, myBLACK);  // Fill black in the rectangle before writing new things
  //  uint16_t NoticeLength = Notice.length();       //get the char length of the string
  display.setTextSize(1);
  display.setTextColor(statascolor);

  display.setFont(&FreeSansBold9pt7b); // Stylish Font
  display.setCursor(sxpos, 59);        // Set From where the text will start apppearng(x,y);
  display.print(presence);             // IN/Busy/Out
  // display.setTextColor(noticecolor);
  display.setFont();

  if (statas == 1)
  {
    display.setTextColor(statascolor, myBLACK); // Set color of individual notice.
    display.setCursor(5, 55);
    display.print(Innotice); // Print the Notice.
  }
  if (statas == 2)
  {
    display.setTextColor(statascolor, myBLACK); // Set color of individual notice.
    display.setCursor(1, 55);
    display.print(Busynotice); // Print the Notice.
  }
  if (statas == 3 && disp_data_notice.length() <= 11) // static notice message.
  {
    display.setTextColor(statascolor, myBLACK); // Set color of individual notice.
    display.setCursor(3, 55);
    display.print(disp_data_notice);
  }

  display.display();
} // End of function
void doubleclick1(uint8_t ofset)
{
  /* Increase the statas by 1 increament only when it is called by the double press function,
  else just call the function.
  */
  statas = statas + 1 - ofset;

  if (statas > 3)
  { // status should come back to In after pressing Out
    statas = 1;
  }

  if (statas == 1)
  {
    disp_data_attendance = "present";
    inbusyout("  IN", statasincolor, 82); // Update [In] in Inbusyout function
  }
  if (statas == 2)
  {
    disp_data_attendance = "onfield";
    inbusyout("BusY", statasbusycolor, 80); // call inbusyout function and update the following parameters.
  }
  if (statas == 3)
  {
    disp_data_attendance = "absent";
    inbusyout("OUT", statasoutcolor, 83); // call inbusyout function and update the following parameters.
    if (disp_data_notice.length() > 11)
    {
      scrolling_notice(disp_data_notice); // call scrolling notice function from below.
    }
  }
  // Serial.println("Attendance: " + disp_data_attendance);
}
void doubleclick() // This function is initialized when the button is pressed twice.
{
  doubleclick1(0);        // bypass this function to anther with a offset
  display.clearDisplay(); // clear the screen
  Serial.println("The double press function has been initiated");
}

/********************************************************************************************************************/

/******************************************************************************************************/
void printmail(String gmail)
{ //[Scrolling Email]

  uint16_t gmaillength = gmail.length();       // Get the sting count of a number
  display.setTextWrap(false);                  // we don't wrap text so it scrolls nicely
  display.setTextColor(contactcolor, myBLACK); // Set text color with black background

  String disp_mail; //
  disp_mail = gmail.substring(0, 11 + mailcharcount);

  if ((millis() - start_time1) >= scrollinterval) // update screen every 100ms
  {
    if (expos >= -(gmaillength * 5 - 6 * 6)) // Asuming 6 pixel average character width
    {
      display.setCursor(expos, 45);
      display.println(disp_mail); // Display shortened email so that it wont overlap with In/busy/out
      display.display();

      expos--; // Decrease the x-coordinate by 1
    }
    start_time1 = millis();
  }

  // for every 6 led scroll of 6, add a string to the main string.
  if (expos - current_expos == -6)
  {
    current_expos = expos;
    mailcharcount++;
  }

  /******************************************************************************************************/

  // Clear the display After the email has been displayed.
  if (expos <= -(gmaillength * 5 - 6 * 6))
  {
    start_time = millis();
    display.clearDisplay();
    // Serial.print("debug");
    expos = 5;         // set the default values
    current_expos = 5; // set the default values
    mailcharcount = 1;
  }
}

/******************************************************************************************************/
void drawborder()
{

  display.setTextWrap(false);
  display.drawRoundRect(0, 0, 128, 64, 3, bordercolor); // Display Border
  display.drawRoundRect(0, 0, 128, 43, 3, bordercolor); // Draw Round rect in Name and Designation
  display.drawFastHLine(0, 42, 128, bordercolor);       // Line After Designation.
  //   display.drawFastHLine(0,49, 128, bordercolor);               //Line After Status
  display.display();
  display.drawRoundRect(127 - 48, 43, 48, 20, 4, inbusyoutcolor);                     // Rectangle in BUSY. (x pos = Text X-pos - 4)
  display.drawRoundRect(127 - 48 + 1, 43 + 1, 48 - 2, 20 - 2, 4 - 1, inbusyoutcolor); // Rectangle in BUSY.
  display.display();
}
void printText()
{

  // To get Length of Strings
  uint8_t NameLength = disp_data_firstname.length();
  uint8_t lastnameLength = disp_data_lastname.length();
  uint8_t DesignationLength = disp_data_designation.length();

  uint8_t Nxpos = matrix_width / 2 - (NameLength * 10) / 2; //(to Centre Align the text)//Coordiate for Name
  uint8_t lxpos = matrix_width / 2 - (lastnameLength * 10) / 2;
  uint8_t Dxpos = matrix_width / 2 - (DesignationLength * 6) / 2; //(to Centre Align the text)//Coordiate for Designation

  display.setFont(&FreeSansBold9pt7b); // Stylish Font
  display.setTextSize(1);              // Text size 8px*10px
  display.setTextColor(namecolor);
  display.setCursor(Nxpos, 14);       // Set From where the text will start apppearng(x,y);
  display.print(disp_data_firstname); // Print Name
  display.display();                  // send over the data to display

  display.setCursor(lxpos, 30);      // Set From where the text will start apppearng(x,y);
  display.print(disp_data_lastname); // Print Name
  display.display();

  display.setFont();
  display.setTextSize(1);
  display.setTextColor(designationcolor);
  display.setCursor(Dxpos, 34); // Set From where the text will start apppearng(8-+x,y); Because it will be desplayed on lower screen.
  display.print(disp_data_designation);
  display.display();
}
void printcontacts(String details)
{ // Print multiple contacts periodically.
  display.setTextColor(contactcolor, myBLACK);
  display.setCursor(5, 45);
  display.print(details); // Print the first contact Info
  display.display();
  yield();
}
void scrolling_notice(String Snotice)
{ // to scroll the received notice.

  uint16_t noticelength = Snotice.length();   // Get the sting count of a number
  display.setTextWrap(false);                 // we don't wrap text so it scrolls nicely
  display.setTextColor(noticecolor, myBLACK); // Set the Desired color of the text

  String disp_notice; // This would store the shortened notice message
  disp_notice = Snotice.substring(0, 11 + noticecharcount);

  if ((millis() - start_time2) >= scrollinterval) // update screen every 150ms
  {
    if (noticexpos >= -(noticelength * 5 - 6 * 5)) // Asuming 5 pixel average character width
    {
      display.setCursor(noticexpos, 55);
      display.println(disp_notice); // Display shortened email so that it wont overlap with In/busy/out
      display.display();
      noticexpos--; // Decrease the x-coordinate by 1px
    }
    start_time2 = millis();
  }

  if (noticexpos - current_noticexpos == -6) // for 5 steps of scrolling, add 1 string to the notice
  {
    current_noticexpos = noticexpos;
    noticecharcount++;
  }

  /******************************************************************************************************/

  // Clear the display After the email has been displayed.
  if (noticexpos <= -(noticelength * 5 - 6 * 5))
  {
    noticexpos = 5;         // set the default values
    current_noticexpos = 5; // set the default values
    noticecharcount = 1;
    // display.clearDisplay();  //Clear the display;
  }
}

void setup()
{
  // Initialize Serial port
  Serial.begin(115200);
  while (!Serial)
    continue;

  display.begin(32); // 1/32 scan display
  pinMode(btn, INPUT);
  pinMode(ldr, INPUT);

  // Push Button Events
  // button.attachClick(singleclick);         // link the function to be called on a singleclick event.
  button.attachDoubleClick(doubleclick); // link the function to be called on a doubleclick event.
  // button.attachLongPressStart(hotspot);      // link the function to be called on a longpress event.
  button.setPressTicks(3000); // Consider long clicks when button is held for 3 seconds

  // Read the data from flash memory
  storage.begin("Data_vault", false); // false->, true -> Read only
  disp_data_firstname = storage.getString("firstname", "");
  disp_data_lastname = storage.getString("lastname", "");
  disp_data_designation = storage.getString("designation", "");
  disp_data_contact1 = storage.getString("contact1", "");
  disp_data_contact2 = storage.getString("contact2", "");
  disp_data_email = storage.getString("email", "");
  disp_data_email = storage.getString("notice", "");

  wifi_connect(); // call the wifi connect function

  // use this on the ESP32
  https.setInsecure();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("Wifi is connected");
    makeHTTPRequest("GET", false, String(get_URI));
    Serial.println("Http Get successful");
  }

  Serial.println("Name: " + data_firstname + " Length: " + (data_firstname.length()));
  Serial.println(data_lastname + " Length: " + (data_lastname.length()));
  Serial.println("Designation: " + data_designation + " Length: " + data_designation.length());
  Serial.println("Phone: " + data_phone);
  Serial.println("Email: " + data_email + "Length: " + data_email.length());
  Serial.print("Attendence: ");
  Serial.println(data_attendance);
  Serial.println("Notice: " + data_outNote);
  Serial.println(" ");
  Serial.println(" ");

  display.clearDisplay(); // Clear what was previously displayed in the Panel
  yield();

  start_time = millis();
  start_time1 = millis();   //[Email Scrolling delay]
  start_time2 = millis();   //[Scrolling notice message]
  httpget_timer = millis(); // to get Http Requests
  doubleclick1(1);          // to display the 'In' status on bootup
}

void loop()
{

  button.tick(); //[Run One Button function]
  printText();   // this function prints name and designation.
  drawborder();  // this function draws all lines and borders
  doubleclick1(1);

  // Display the COntact Info on the display
  if ((millis() - start_time) <= interval)
  {
    printcontacts(disp_data_contact1); // Call function and display First
  }
  if ((millis() - start_time) > interval && (millis() - start_time) <= (interval * 2) && (disp_data_contact2.length() >= 8))
  {
    printcontacts(disp_data_contact2); // Call function and display First
  }
  if ((millis() - start_time) > (interval * 2) || ((millis() - start_time) > interval && disp_data_contact2.length() < 8))
  {
    printmail(disp_data_email); // Call function and display First
  }

  /*
    if (millis() - start_timer >= 20000)
    {
      counter++;
      start_timer = millis();

      if (counter == 1)
      {
        Presence_status = "Present";
        queryString = String("?attendances=") + String(Presence_status) + String("&deviceId=") + String(Device_ID);
        makeHTTPRequest("POST", true, String(post_URI));
      }
      if (counter == 2)
      {
        Presence_status = "Absent";
        queryString = String("?attendances=") + String(Presence_status) + String("&deviceId=") + String(Device_ID);
        makeHTTPRequest("POST", true, String(post_URI));
      }
      if (counter == 3)
      {
        Presence_status = "onfield";
        queryString = String("?attendances=") + String(Presence_status) + String("&deviceId=") + String(Device_ID);
        makeHTTPRequest("POST", true, String(post_URI));
        counter = 0;
        Serial.println();
      }
    }
    */
  if (WiFi.status() == WL_CONNECTED && current_statas != statas)
  {
    current_statas = statas;
    if (statas == 1)
    {
      Presence_status = "Present";
      queryString = String("?attendances=") + String(Presence_status) + String("&deviceId=") + String(Device_ID);
      makeHTTPRequest("POST", true, String(post_URI));
    }
    if (statas == 2)
    {
      Presence_status = "onfield";
      queryString = String("?attendances=") + String(Presence_status) + String("&deviceId=") + String(Device_ID);
      makeHTTPRequest("POST", true, String(post_URI));
    }
    if (statas == 3)
    {
      Presence_status = "absent";
      queryString = String("?attendances=") + String(Presence_status) + String("&deviceId=") + String(Device_ID);
      makeHTTPRequest("POST", true, String(post_URI));
      Serial.println();
    }
  }
  // http get at the regular intervals of 15 sec
  if (millis() - httpget_timer >= httpget_interval && (WiFi.status() == WL_CONNECTED))
  {
    httpget_timer = millis();
    makeHTTPRequest("GET", false, String(get_URI));
    Serial.println("Name: " + data_firstname + " Length: " + (data_firstname.length()));
    Serial.println(data_lastname + " Length: " + (data_lastname.length()));
    Serial.println("Designation: " + data_designation + " Length: " + data_designation.length());
    Serial.println("Phone: " + data_phone);
    Serial.println("Email: " + data_email + "Length: " + data_email.length());
    Serial.println("Attendence: " + data_attendance + "Length: " + data_attendance.length());
    Serial.println("Attendence: " + disp_data_attendance + "Length: " + data_attendance.length());
    Serial.println("Notice: " + data_outNote);
    Serial.println(" ");
    Serial.println(" ");
  }
}
