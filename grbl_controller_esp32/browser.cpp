// to do : 
//         remove Serial
//         run wifi in the same core as normal wifi core in a separate task (can perhaps gives an issue for sharing SPI)
//         allows to configure and run in AP mode instead of STA
//         implement buttons and remove button Home


#include <SPI.h>
#include "SdFat.h"
#include <WiFi.h>              // Built-in
#include <WebServer.h>
#include "config.h"
#include "language.h"
#include "browser.h"
#include <Preferences.h>
#include "draw.h"
#include "actions.h"
#include "setupTxt.h"
String  webpage = "";
WebServer server(80);

extern SdFat32 sd;
extern uint8_t wifiType ; // can be NO_WIFI(= 0), ESP32_ACT_AS_STATION(= 1), ESP32_ACT_AS_AP(= 2)
char wifiPassword[65] ;
char wifiSsid[65] ;
IPAddress local_IP; // create IP adress with 4 values = 0; will be filled in init
IPAddress gateway; // create IP adress with 4 values = 0; will be filled in init
IPAddress subnet; // create IP adress with 4 values = 0; will be filled in init
IPAddress grbl_Telnet_IP; // create IP adress with 4 values = 0; will be filled in init
String local_IPStr = ""; // used to store the IP address values in string (for SD, preferences, config) 
String gatewayStr = ""; // used to store the IP address values in string (for SD, preferences, config)
String subnetStr = ""; // used to store the IP address values in string (for SD, preferences, config)
String grbl_Telnet_IPStr =  ""; // used to store the GRBL IP address values in string (for SD, preferences, config)
extern Preferences preferences ; // used to save the WIFi parameters  
extern volatile uint8_t statusPrinting  ;


extern float runningPercent ;
extern uint32_t sdFileSize ;
extern uint32_t sdNumberOfCharSent ;

File32 root ; // used for Directory 

String onetimeMessage = "";

void initWifi() {
  uint32_t startMillis = millis();
  uint8_t yText = 140 ;
  drawLogo();
  retrieveWifiParam();
  if (wifiType == NO_WIFI) {
    drawLineText( __WIFI_NOT_REQUESTED , hCoord(160), vCoord(yText), 2 , 1 , TFT_GREEN) ; // texte, x, y , font, size ,color  
      //return ; 
  } else {
    grbl_Telnet_IP.fromString(grbl_Telnet_IPStr); // convert telnet ip adr
    if ( local_IPStr != "" && gatewayStr != "" && subnetStr != "" ){
      local_IP.fromString(local_IPStr);
      gateway.fromString(gatewayStr);
      subnet.fromString(subnetStr);
  
      if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("[MSG:Fix IP address failed to configure]");
      }  
    } else {
      Serial.println("[MSG:Wifi start without satic IP address]");
    }
    if (wifiType == ESP32_ACT_AS_STATION) {
        //Serial.print("local IP="); Serial.println(local_IPStr);
        //Serial.print("gateway="); Serial.println(gatewayStr);
        //Serial.print("subnet="); Serial.println(subnetStr);
        //blankTft("Connecting to Wifi access point", 5 , 20 ) ; // blank screen and display a text at x, y 
        WiFi.begin(wifiSsid , wifiPassword);
        uint8_t initWifiCnt = 40 ;   // maximum 40 retries for connecting to wifi
        while (WiFi.status() != WL_CONNECTED)  { // Wait for the Wi-Fi to connect; max 40 retries
          delay(250); // Serial.print('.');
          if ((initWifiCnt % 8) == 0) {
            drawLineText( __CONNECTING_TO_WIFI  , hCoord(160), vCoord(yText), 2 , 1 , TFT_GREEN) ; // texte, x, y , font, size ,color  
          }else if ((initWifiCnt % 8) == 5) {
            clearLine(vCoord(yText), 2 , 1, SCREEN_BACKGROUND);
          }
          //printTft(".") ;
          initWifiCnt--;
          if ( initWifiCnt == 0) {
            fillMsg(_WIFI_NOT_FOUND  );
            break;
          }
        }
        clearLine(vCoord(yText), 2 , 1, SCREEN_BACKGROUND);
        if (initWifiCnt == 0){
          drawLineText( __NO_WIFI_CONNECTION , hCoord(160), vCoord(yText), 2 , 1 , SCREEN_ALERT_TEXT) ; // texte, x, y , font, size ,color  
        } else {
          drawLineText( __CONNECTED_AS_STATION  , hCoord(160), vCoord(yText), 2 , 1 , TFT_GREEN) ; // texte, x, y , font, size ,color  
        }
        delay(2000) ;
        //Serial.println("\nConnected to "+WiFi.SSID()+" Use IP address: "+WiFi.localIP().toString()); // Report which SSID and IP is in use
    } else if (wifiType == ESP32_ACT_AS_AP) {
      WiFi.softAP( wifiSsid , wifiPassword );
      //Serial.println("\nESP has IP address: "+ WiFi.softAPIP().toString()); // Report which SSID and IP is in use
          drawLineText( __AP_STARTED  , hCoord(160), vCoord(yText), 2 , 1 , TFT_GREEN) ; // texte, x, y , font, size ,color  
    }  
    //WiFi.setSleep(false);
    //----------------------------------------------------------------------   
    ///////////////////////////// Server Commands 
    server.on("/",         HomePage);
    server.on("/download", File_Download);
    server.on("/fupload",  HTTP_POST,[](){ server.send(200,"text/plain","");}, handleFileUpload);
    server.on("/delete",   File_Delete);
    server.on("/browse",      sd_dir);
    server.on("/confirmDelete",      confirmDelete);
    server.on("/status", serverStatus);
    server.on("/execute", serverExecuteFile);
    server.on("/createDir", serverCreateDir);
    server.on("/fileupload.js", file_fileupload_js);
    server.on("/style.css", file_style_css);
    ///////////////////////////// End of Request commands
    server.begin();
    //Serial.println("HTTP server started");  
  }
  while (millis() < (startMillis + 4000) ) {
    delay(100);
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void retrieveWifiParam(void){  // get the wifi parameters (type, SSID, password)
              // parameters are stored in preferences
              // when a file named wifi.cfg exist on SD card, those parameters are used and saved in preferences
              // else if parameters exist in preferences, we use them
              // else use those from config (not saved)
    char myPassword[] = MY_PASSWORD ;
    char mySsid[] = MY_SSID ;
    if ( checkWifiOnSD() ) {   // true when SD contains valid parameters
      // save parameters in preferences
      preferences.putChar("WIFI", wifiType ) ;
      preferences.putString("PASSWORD", wifiPassword ) ;
      preferences.putString("SSID", wifiSsid ) ;
      preferences.putString("LOCAL_IP", local_IPStr ) ;
      preferences.putString("GATEWAY", gatewayStr ) ;
      preferences.putString("SUBNET", subnetStr ) ;
      preferences.putString("SUBNET", subnetStr ) ;
      preferences.putString("GRBL_TELNET_IP", grbl_Telnet_IPStr ) ;
      preferences.putChar("wifiDef", 1); // 1 is used to say that a set of wifi preferences is saved
      Serial.println("[MSG:using SD param for Wifi]") ;
    } else if ( preferences.getChar("wifiDef", 0) ) {  // return 0 when no set is defined and 1 when wifi is defined in preferences
      // search in preferences
      wifiType = preferences.getChar("WIFI", NO_WIFI  ) ; // if key does not exist return NO_WIFI
      preferences.getString("PASSWORD" , wifiPassword , sizeof(wifiPassword)  ) ;
      preferences.getString("SSID", wifiSsid , sizeof(wifiSsid) ) ;
      local_IPStr = preferences.getString("LOCAL_IP" , "") ; // return a string if found, otherwise the null string
      gatewayStr = preferences.getString("GATEWAY" , "") ; // return a string if found, otherwise the null string
      subnetStr = preferences.getString("SUBNET" , "") ; // return a string if found, otherwise the null string
      grbl_Telnet_IPStr = preferences.getString("GRBL_TELNET_IP" , "") ; // return a string if found, otherwise the null string
      Serial.println("[MSG:using preference param for WiFi]") ;
    } else {
      // use those from config.h
      wifiType = WIFI ;
      strcpy(wifiPassword, myPassword) ;
      strcpy(wifiSsid, mySsid) ;
      Serial.println("[MSG:using config.h param for WiFi]") ;
    #ifdef LOCAL_IP
      local_IPStr = LOCAL_IP ;
    #endif
    #ifdef GATEWAY
      gatewayStr = GATEWAY ;
    #endif
    #ifdef SUBNET
      subnetStr = SUBNET ;
    #endif
    #ifdef GRBL_TELNET_IP
      grbl_Telnet_IPStr = GRBL_TELNET_IP ;
    #endif     
    }
    //Serial.print("wifiType=") ; Serial.println(wifiType) ;
    //Serial.print("wifiPassword=") ; Serial.println(wifiPassword) ;
    //Serial.print("wifiSsid=") ; Serial.println(wifiSsid) ;
    //Serial.print("local_IPStr=") ; Serial.println(local_IPStr) ;
    //Serial.print("gatewayStr=") ; Serial.println(gatewayStr) ;
    //Serial.print("subnetStr=") ; Serial.println(subnetStr) ;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// retieve wifi parameters from SD card in a file wifi.cfg
// return true if correct parameters are found
boolean checkWifiOnSD(void){
      // return true if a valid file wifi.cfg exist on the SD card; if so we use those values
      SdBaseFile wifiFile ;
      char line[100] ;  //buffer to get a line from SD
      bool wifiTypeOk = false;
      bool wifiPasswordOk = false;
      bool wifiSsidOk = false;
      //bool local_IPOk = false ;
      //bool gatewayOk = false ;
      //bool subnetOk = false ;
      //bool grbl_Telnet_IPOk = false ;  
      uint8_t n; // number of bytes in a line
      char * pBeginValue ;
      char * pEndValue ;
      uint8_t sizeValue ;
      char wifiTypeString[30] ;
      char local_IPChar[50] = {0} ;
      char gatewayChar[50] = {0} ;
      char subnetChar[50] = {0} ;
      char grbl_Telnet_IPChar[50] = {0};        
      if ( ! sd.begin(SD_CHIPSELECT_PIN , SD_SCK_MHZ(5)) ) {  
          Serial.println( __CARD_MOUNT_FAILED  ) ;
          Serial.println(sd.card()->errorCode());
          return false;       
      }
      //if ( ! SD.exists( "/" ) ) { // check if root exist
      if ( ! sd.exists( "/" ) ) { // check if root exist   
          //Serial.println( __ROOT_NOT_FOUND  ) ;
          return false;  
      }
      if ( ! sd.chdir( "/" ) ) {
          //Serial.println( __CHDIR_ERROR  ) ;
          return false;  
      }
      if ( ! wifiFile.open("/wifi.cfg" ) ) { // try to open wifi.cfg 
          //Serial.println("failed to open calibrate.txt" ) ;
          return false;  
      }
      //Serial.println("wifi.cfg exist on SD" ) ;
      //Serial.print("sizeof line=") ; Serial.println(sizeof(line));      
      // read the file line by line
      while ( (n = wifiFile.fgets(line, sizeof(line)-2)) > 0) {
        //Serial.print("line=");  Serial.println(line) ;
        line[n+1] = 0;  // add a end of string code in order to use strrchr
        pBeginValue = strchr(line,'"'); //search first "
        pEndValue = strrchr(line,'"'); //search last "
        if ( (pBeginValue !=NULL) && ( pEndValue != NULL) ) {
          if ( pEndValue > pBeginValue) {
            *pEndValue = 0 ;
            sizeValue = pEndValue - pBeginValue -1;  // number of char between the ""
            if ( memcmp ( "WIFI=", line, sizeof("WIFI=")-1) == 0){
              wifiTypeOk = true;
              memcpy(wifiTypeString , pBeginValue+1 , sizeValue) ;
              if (memcmp ( "NO_WIFI", wifiTypeString ,sizeof("NO_WIFI")-1  )== 0) {
                wifiType = NO_WIFI ; 
              } else if (memcmp ( "ESP32_ACT_AS_STATION", wifiTypeString ,sizeof("ESP32_ACT_AS_STATION")-1  )== 0) {
                wifiType = ESP32_ACT_AS_STATION ; 
              } else if (memcmp ( "ESP32_ACT_AS_AP", wifiTypeString ,sizeof("ESP32_ACT_AS_AP")-1  )== 0) {
                wifiType = ESP32_ACT_AS_AP ; 
              } else {
                wifiTypeOk = false;
              }
            } else if ( memcmp ( "PASSWORD=", line, sizeof("PASSWORD=")-1) == 0){
              memcpy(wifiPassword , pBeginValue+1 , sizeValue) ; 
              wifiPasswordOk = true;
            } else if ( memcmp ( "SSID=", line, sizeof("SSID=")-1) == 0){
              memcpy(wifiSsid , pBeginValue+1 , sizeValue) ;
              wifiSsidOk = true ;
            } else if ( memcmp ( "LOCAL_IP=", line, sizeof("LOCAL_IP=")-1) == 0){
              memcpy(local_IPChar , pBeginValue+1 , sizeValue) ;
              local_IPStr = local_IPChar ;
              //local_IPOk = true ;
            } else if ( memcmp ( "GATEWAY=", line, sizeof("GATEWAY=")-1) == 0){
              memcpy(gatewayChar , pBeginValue+1 , sizeValue) ;
              gatewayStr = gatewayChar ;
              //gatewayOk = true ;
            } else if ( memcmp ( "SUBNET=", line, sizeof("SUBNET=")-1) == 0){
              memcpy(subnetChar , pBeginValue+1 , sizeValue) ;
              subnetStr = subnetChar ;
              //subnetOk = true ;
            } else if ( memcmp ( "GRBL_TELNET_IP=", line, sizeof("GRBL_TELNET_IP=")-1) == 0){
              memcpy(grbl_Telnet_IPChar , pBeginValue+1 , sizeValue) ;
              grbl_Telnet_IPStr = grbl_Telnet_IPChar ;
              //grbl_Telnet_IPOk = true ;
            }  
          }
        }
      }
      wifiFile.close() ;
      if ( wifiTypeOk && wifiPasswordOk && wifiSsidOk ) {
        return true ;
      } else {
        return false ;
      }  
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void P( char * text) {    // used to debug; print a text and a new line
  Serial.println(text);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void processWifi(void){
  server.handleClient(); // Listen for client connections
}

boolean getWifiIp( char * ipBuf ) {          // return true if wifi status = connected
  if (wifiType == ESP32_ACT_AS_STATION ) {
    if (WiFi.status() == WL_CONNECTED ) {
      strcpy( ipBuf , WiFi.localIP().toString().c_str() ) ;
      return true ;
    } else {
      ipBuf[0] = 0 ;
      return false ;   
    }
  } else if (wifiType == ESP32_ACT_AS_AP ) {
    strcpy( ipBuf , WiFi.softAPIP().toString().c_str() ) ;
    return true ;
  } 
  return false ;  
}

// All supporting functions from here...
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void HomePage(){
  SendHTML_Header();
  webpage += F("<a href='/browse'><button style='font-size: 75%'>Browse Files</button></a><br>");
  webpage += F("<a href='/status'><button style='font-size: 75%'>Status (json)</button></a><br>");
  
//  webpage += F("<a href='/download'><button>Download</button></a>");
//  webpage += F("<a href='/upload'><button>Upload</button></a>");
//  webpage += F("<a href='/stream'><button>Stream</button></a>");
//  webpage += F("<a href='/delete'><button>Delete</button></a>");
//  webpage += F("<a href='/dir'><button>Directory</button></a>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop(); // Stop is needed because no content length was sent
}
String getParent(String path)
{
  if(path.length()<1)
    return "/";
  return path.substring(0, path.lastIndexOf("/", path.length()-(path.charAt(path.length()-1)=='/'?2:1)));
}
//--------------------------------------------------------------------------
boolean checkSd() {
  if ( ! sd.begin(SD_CHIPSELECT_PIN , SD_SCK_MHZ(5)) ) {  
      reportError("Fail to mount SD card - SD card present?" );
      return false;
  }
  if ( ! sd.exists( "/" ) ) { // check if root exist 
     reportError("Root ('/') not found on SD card");   
     return false;
  }
  return true ;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void confirmDelete(){ 
  if (server.args() > 0 ) { // check that arguments were received
    if (server.hasArg("fileName")) SelectInput("Confirm filename to delete","delete","delete" , server.arg(0)); 
  }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Download(){ // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
  if (server.args() > 0 ) { // Arguments were received
    if (server.hasArg("file")) 
      DownloadFile(server.arg("file"));
  }
  else SelectInput("Enter filename to download","download","download","");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void DownloadFile(String filename){
  //Serial.print("Download file: ") ;Serial.println(  filename ) ;
  //String fileNameS ;
  //fileNameS = "/" + filename ;
  //const char  * fileNameC ;
  //fileNameC = fileNameS.c_str() ;
  if (checkSd() ) { 
    if (sd.exists( filename.c_str() ) ) {
      //Serial.println("Open file") ;
      File32 download ;
      download = sd.open( filename.c_str() );
      if (download) {
        //Serial.println("File open successfully") ;
        server.sendHeader("Content-Type", "text/text");
        server.sendHeader("Content-Disposition", "attachment; filename="+filename);
        server.sendHeader("Connection", "close");
        server.streamFile(download, "application/octet-stream");
        download.close(); 
      } else {
        //Serial.println("File not opened") ;
        reportError("Error : could not open file to dowload"); 
      }
      download.close() ;
    } else reportError( "Error: file to download did not exist");
  } 
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void serverCreateDir(){
  if (!server.hasArg("directoryName"))
  { 
//      SelectInput("Name of directory to create","/createDir","def" , server.arg(0)); 
      SendHTML_Header();
      webpage += F("<h3>Name of directory to create</h3>"); 
  webpage += F("<FORM action='/createDir' method='post'>");
  webpage += F("<input type='hidden' name='parentDir' value='"); webpage +=server.arg("parentDir"); webpage +=F("'>"); 
  webpage += F("<input type='text' name='directoryName' value=''>"); 
  webpage += F("<input type='submit' name='Create' value='Submit' class='buttons' ><br>");
 // webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();

  }
  else
  {
    Serial.print("create dir: ");
    Serial.print(server.arg("parentDir"));
    Serial.print(", ");
    Serial.println(server.arg("directoryName"));

    if ( ! sd.begin(SD_CHIPSELECT_PIN , SD_SCK_MHZ(5)) ) {  
            reportError("Fail to mount SD card - SD card present?" );
        } else { 
          String path = String(server.arg("parentDir"))+"/"+String(server.arg("directoryName"));
          if(sd.mkdir(path))
          {
            onetimeMessage = F("<h3>Directory sucessfully created</h3>");
          }
          else
          {
              onetimeMessage = F("Failed to create directory");
          }
          webpage = "<head><meta http-equiv=\"refresh\" content=\"0; URL=/browse?dir=" + path + "\"/></head>";
          server.send(200, "text/html", webpage);
        }  
  }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
File32 UploadFile;
boolean errorWhileUploading ; 
void handleFileUpload(){ // upload a new file to the Filing system
  //if (checkSd() ) {
  //if ( true ) {
      HTTPUpload& uploadfile = server.upload(); // See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
                                                // For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
      if(uploadfile.status == UPLOAD_FILE_START) {
        errorWhileUploading = false ;
        Serial.print(F("filename "));
        Serial.println(uploadfile.filename.c_str());
        Serial.print(F("server.hasArg(dir) "));
        Serial.println(server.hasArg("dir"));        
        
        String filename = uploadfile.filename;
        
        if(server.hasArg(F("dir")))
        {
          filename = server.arg("dir")+"/"+filename;
        }
        Serial.print(F("File upload to "));
        Serial.println(filename.c_str());
        sd.remove(filename.c_str());                  // Remove a previous version, otherwise data is appended the file again
        UploadFile.close() ;
        //UploadFile = sd.open(filename.c_str() , 0X11);  // Open the file for writing in SPIFFS (create it, if doesn't exist)
        //UploadFile = sd.open(filename.c_str() , O_WRITE | O_CREAT);  // Open the file for writing (create it, if doesn't exist)
        if ( ! sd.begin(SD_CHIPSELECT_PIN , SD_SCK_MHZ(5)) ) { 
            sendHtmlMessage(409, "Fail to mount SD card - SD card present?");
            errorWhileUploading = true ;
        } else {     
          Serial.print("Creating file ok: ");
          UploadFile = sd.open(filename.c_str() , O_WRITE | O_CREAT);  // Open the file for writing (create it, if doesn't exist)
          if ( ! UploadFile ) errorWhileUploading = true ;
          Serial.println(!errorWhileUploading);
        }  
      } else if (uploadfile.status == UPLOAD_FILE_WRITE)  {
        Serial.print("Uploading file ok. chunk: ");
        if(UploadFile) { 
          int32_t bytesWritten = UploadFile.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
          if ( bytesWritten == -1) errorWhileUploading = true ; 
        } else { 
          errorWhileUploading = true ;
        }
        Serial.print(uploadfile.currentSize);
        Serial.print(" bytes. status: ");
        Serial.println(!errorWhileUploading);
      } else if (uploadfile.status == UPLOAD_FILE_END) {
        if(UploadFile && ( errorWhileUploading == false) )          // If the file was successfully created
        {                                    
          UploadFile.close();   // Close the file at the end
          onetimeMessage="File was successfully uploaded. Fileame: "+uploadfile.filename+", size: "+file_size(uploadfile.totalSize);
          sendHtmlMessage(200, onetimeMessage);
          Serial.println("File closed");
        } 
        else
        {
          UploadFile.close(); // close for safety ; not sure it is really needed
          sendHtmlMessage(409, "Could Not Create Uploaded File (write-protected?)");
        }
      }
      else if (uploadfile.status == UPLOAD_FILE_ABORTED)
      {
          Serial.println("File upload aborted!");
          onetimeMessage = "File upload was aborted!";
      }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void sd_dir(){ 
  if (checkSd() ) { 
    String dir = "";
    if (server.args() > 0 && server.hasArg("dir")) 
    {
        dir = server.arg("dir");        
    }
    if(dir.length()<1)
      dir = "/";
      
    root.close() ;
    root = sd.open(dir.c_str());
    if (root) {
      root.rewind();
      SendHTML_Header();
      //webpage += F("<h3 class='rcorners_m'>SD Card Contents</h3><br>");
      webpage += "<h3>"+dir+"</h3><br>";
      if (onetimeMessage.length() > 0)
      {
          webpage += "<p>" + onetimeMessage + "</p>";
          onetimeMessage = "";
      }
      webpage += F("<div>");
      webpage += F("<FORM enctype='multipart/form-data' id='upload' onsubmit='return false;'>");
      webpage += "<input type='hidden' name='dir' id='dir' value='"+dir+"'>";
      webpage += F("<input class='buttons' style='width:80%' type='file' name='fupload' id = 'fupload' value=''>");
      webpage += F("<button class='buttons' style='width:20%' onclick=\"uploadFile()\">Upload</button><br>");
      webpage += F("<progress id='progressBar' value='0' max='100' style='width:300px;'></progress>");
      webpage += F("<h3 id='status'></h3>");
      webpage += F("<p id='loaded_n_total'></p>");

      webpage += F("</form>");
      webpage += "<a href=\"/createDir?parentDir="+dir+"\"><button style='font-size:85%'>Create dir</button></a>";
      webpage += F("</div>");
      webpage += F("<table align='center'>");
      webpage += F("<tr><th>Name/Type</th><th style='width:20%'>Type File/Dir</th><th>File Size</th><th>Delete</th><th>Download</th></tr>");
      printDirectory(dir.c_str(),0);
      webpage += F("</table>");
      SendHTML_Content(); 
    } else {
      SendHTML_Header();
      webpage += F("<h3>No Files Found</h3>");
      SendHTML_Content();
    }
    root.close();
    append_page_footer();
    SendHTML_Content();
    SendHTML_Stop();   // Stop is needed because no content length was sent
  } 
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void printDirectory(const char * dirname, uint8_t levels){
  File32 root1 = sd.open(dirname);
  String directory = String(dirname);
  if(!directory.endsWith("/"))
    directory = directory+"/";
  char fileName[33] ;
  //String fileNameS ;
  if(!root1){
    root1.close() ;
    return;
  }
  if(!root1.isDir()){
    root1.close() ;
    return;
  }
  File32 file1 ;

  if(directory.length()>1)
  {
    String parentDir = getParent(directory);
//    String parentDir = directory.substring(0, directory.lastIndexOf("/", directory.length()-2));
    if(parentDir.length()==0)
      parentDir = "/";
    webpage += "<tr><td><a href=\"/browse?dir="+parentDir+"\">..</td><td>Dir</td><td></td><td></td><td></td></tr>";
            //printDirectory(fileName, levels-1);
  }
  root1.rewind();
  //P("print Directory:  dirname is a directory") ;
  while(file1.openNext(&root1)){
    if (webpage.length() > 1000) {
      SendHTML_Content();
    }
    file1.getName(fileName , 32);
    if(file1.isDir())
    {
      if(strcmp(fileName, "System Volume Information")==0)
      {
        // Skip this directory
      }
      else
      {
        webpage += "<tr><td><a href=\"/browse?dir="+directory+String(fileName)+"\">" +String(fileName)+ "</td><td>Dir</td><td></td>";
        webpage += "<td><form action='/confirmDelete'> <input type='hidden' name='fileName' value='" + directory+String(fileName) +  "' ><input type='submit' value='Delete' ></form></td>" ;
      webpage += "<td></td>" ;
      webpage +=  "</tr>";
      }
    }   
    file1.close();   
  }
  root1.rewind();
  while(file1.openNext(&root1)){
    if (webpage.length() > 1000) {
      SendHTML_Content();
    }
    file1.getName(fileName , 32);
    if(!file1.isDir()){
      int bytes = file1.size();
      String fsize = "";
      if (bytes < 1024)                     fsize = String(bytes)+" B";
      else if(bytes < (1024 * 1024))        fsize = String(bytes/1024.0,3)+" KB";
      else if(bytes < (1024 * 1024 * 1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
      else                                  fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
      webpage += "<tr><td>"  + String(fileName) +  "</td><td>File</td><td>" + fsize + "</td>"; //</tr>";
      //<td onClick="location.href='http://www.stackoverflow.com';"> Cell content goes here </td>
      webpage += "<td><form action='/confirmDelete'> <input type='hidden' name='fileName' value='" + directory+String(fileName) +  "' ><input type='submit' value='Delete' ></form></td>" ;
      webpage += "<td><form action='/download'> <input type='hidden' name='file' value='" + directory+String(fileName) +  "' ><input type='submit' value='Download' ></form></td>" ;
      webpage +=  "</tr>";
    }   
    file1.close();   
  }
 
  file1.close();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Delete(){
  if (server.args() > 0 ) { // Arguments were received
    if (server.hasArg("delete")) SD_file_delete(server.arg(0));
  }
  else SelectInput("Select a File to Delete","delete","delete","");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SD_file_delete(String filename) { // Delete the file 
  if (checkSd() ) { 
    File32 dataFile = sd.open( filename.c_str() ); //  
    if (dataFile) {
      if(dataFile.isDir())
      {
        if(sd.rmdir(filename.c_str()))
          onetimeMessage = "<h3>Directory '" +filename+ "' has been erased</h3>"; 
         else
          onetimeMessage = "<h3>Directory '"  +filename+ "' could not be erased! Non empty?</h3>";
      }
      else if (sd.remove( filename.c_str() )) {
        onetimeMessage = "<h3>File '" +filename+ "' has been erased</h3>"; 
      } else { 
        onetimeMessage = "<h3>File '"  +filename+ "' could not be erased !!!!!</h3>";
      }
    } else {
      onetimeMessage = "<h3>File '" +filename+ "' does not exist !!!!!</h3>";
    }
    //webpage = "<head><meta http-equiv=\"refresh\" content=\"0; URL=/browse?dir=" + filename.substring(0, filename.lastIndexOf("/", filename.length() - 2)) + "\"/></head>";
    
    webpage = "<head><meta http-equiv=\"refresh\" content=\"0; URL=/browse?dir=" + getParent(filename) + "\"/></head>";
    server.send(200, "text/html", webpage);
  } 
} 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendCacheHeaders()
{
    server.sendHeader("Cache-Control", "max-age=3600");
}
void SendNoCacheHeaders()
{
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
}
void SendHTML_Header(){
  SendNoCacheHeaders();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); 
  server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves. 
  append_page_header();
  server.sendContent(webpage);
  webpage = "";
}
void SendJson_Header(){
  SendNoCacheHeaders();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); 
  server.send(200, "application/json", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves. 
  webpage = "";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Content(){
  server.sendContent(webpage);
  webpage = "";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Stop(){
  server.sendContent("");
  server.client().stop(); // Stop is needed because no content length was sent
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SelectInput(String heading1, String command, String arg_calling_name, String initialValue){
  SendHTML_Header();
  webpage += F("<h3>"); webpage += heading1 + "</h3>"; 
  webpage += F("<FORM action='/"); webpage += command + "' method='post'>"; // Must match the calling argument e.g. '/chart' calls '/chart' after selection but with arguments!
  webpage += F("<input type='hidden' name='"); webpage += arg_calling_name; webpage += F("' value='");  webpage += initialValue ; webpage += F("'>"); 
  webpage += F("<input type='text' name='fileNameBox' value='");  webpage += initialValue ; webpage += F("' disabled > <br><br>"); 
  webpage += F("<input type='submit' name='"); webpage += arg_calling_name; webpage += F("' value='Submit' class='buttons' ><br><br>");
 // webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void reportError(String textError){
  SendHTML_Header();
  webpage += "<br><br>" + textError; 
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
void sendHtmlMessage(int statuscode, String text)
{
  server.send(statuscode, "text/html","<body><h2>"+text+"</h2></body>");
  server.client().stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportFileNotPresent(String target){
  SendHTML_Header();
  webpage += F("<h3>File does not exist</h3>"); 
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
String file_size(int bytes){
  String fsize = "";
  if (bytes < 1024)                 fsize = String(bytes)+" B";
  else if(bytes < (1024*1024))      fsize = String(bytes/1024.0,3)+" KB";
  else if(bytes < (1024*1024*1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
  else                              fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
  return fsize;
}
float calculateSdPercent()
{
    if(sdFileSize > 0)
    return sdFileSize > 0 ? (float)sdNumberOfCharSent / sdFileSize : 100.0;
}

String getCurrentStatus()
{
  String status = "";
    switch (statusPrinting)
    {
    case PRINTING_STOPPED:
       status = "Idle"; break;
    case PRINTING_FROM_SD:
        status = String("Running program (") + (int)(calculateSdPercent()*100) + "%)"; break;
    case PRINTING_FROM_GRBL:
        status = String("Running program (") + runningPercent + "%)"; break;
    case PRINTING_ERROR:
        status = "Error on program"; break;
    case PRINTING_PAUSED:
    case PRINTING_FROM_GRBL_PAUSED:
        status = "Running program (paused)"; break;
    case PRINTING_FROM_USB:
        status = "Running (USB)"; break;
    case PRINTING_CMD:
        status = "Running command"; break;
    case PRINTING_FROM_TELNET:
        status = "Running (telnet)"; break;
    case PRINTING_STRING:
        status = "Running string"; break;
    default:
        status = "Unknown state";
    }
    return status;
}
void append_page_header() {
  webpage  = F("<!DOCTYPE html><html>");
  webpage += F("<head>");
  webpage += F("<title>File Server</title>"); // NOTE: 1em = 16px
  webpage += F("<meta name='viewport' content='user-scalable=yes,initial-scale=1.0,width=device-width'>");
  webpage += F("<script src='/fileupload.js'></script>");
  webpage += F("<link rel='stylesheet' href='style.css'>");
  webpage += F("</head><body><h1><a href=\"/\" style=\";text-decoration:none;color:inherit\">&#8962;RS-CNC32&nbsp;");
  webpage += F("Current status: ");
  webpage += getCurrentStatus();
  webpage += F("</a></h1>"); 
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void append_page_footer(){ // Saves repeating many lines of code for HTML page footers
  webpage += F("</body></html>");
}

void addJsonAttribute(String name, String value, boolean first)
{
  if(!first) 
      webpage +=", ";
  webpage += "\""+name + "\": \""+value+"\"";
}
void addJsonAttribute(String name, long value, boolean first)
{
  if(!first) 
      webpage +=", ";
  webpage += "\""+name + "\": \""+value+"\"";
}
void addJsonAttribute(String name, float value, boolean first)
{
  if(!first) 
      webpage +=", ";
  webpage += "\"" + name + "\": \""+value+"\"";
}
void serverExecuteFile()
{
  if (server.args() > 0 ) { // Arguments were received
    if (server.hasArg("filename")) 
    {
      if (checkSd() ) { 
      //SendHTML_Header();  
      File32 dataFile = sd.open( server.arg(0).c_str() ); //  
      if (dataFile) {
        webpage = "ok"; 
//        dataFile
      } else { 
        webpage = "not found: "+server.arg(0);
      }
    } else {
      webpage = "Specify filename!";
    }
    }
    
  }
  else SelectInput("Select a File to execute","execute","execute","");  

  SendJson_Header();
  SendHTML_Content();
  SendHTML_Stop(); // Stop is needed because no content length was sent

//  webpage = "ok";
  fSdFilePrint(1);
}

void serverStatus(){
  SendJson_Header();

  webpage = "{";
  String status;
  switch(statusPrinting)
  {
    case PRINTING_STOPPED:
      status = "PRINTING_STOPPED"; break;
    case PRINTING_FROM_SD:
      status = "PRINTING_FROM_SD"; break;
    case PRINTING_ERROR:
      status = "PRINTING_ERROR"; break;
    case PRINTING_PAUSED:
      status = "PRINTING_PAUSED"; break;
    case PRINTING_FROM_USB:
      status = "PRINTING_FROM_USB"; break;
    case PRINTING_CMD:
      status = "PRINTING_CMD"; break;
    case PRINTING_FROM_TELNET:
      status = "PRINTING_FROM_TELNET"; break;
    case PRINTING_STRING:
      status = "PRINTING_STRING"; break;
    case PRINTING_FROM_GRBL:
      status = "PRINTING_FROM_GRBL"; break;
    case PRINTING_FROM_GRBL_PAUSED:
      status = "PRINTING_FROM_GRBL_PAUSED"; break;
    default:
      status = "UNKNOWN";
  }
  addJsonAttribute("status", status, true);
  if(statusPrinting == PRINTING_FROM_SD && sdFileSize>0)
  {
    addJsonAttribute("progress", calculateSdPercent(), false);
    addJsonAttribute("fileSize", (long)sdFileSize, false);
    addJsonAttribute("bytesSent", (long)sdNumberOfCharSent, false); 
  }
  if(statusPrinting == PRINTING_FROM_GRBL && sdFileSize>0)
  {
    addJsonAttribute("progress", runningPercent, false);
  }
  webpage +="}";
  SendHTML_Content();
  SendHTML_Stop(); // Stop is needed because no content length was sent
}
void file_fileupload_js()
{
  webpage = "";
  webpage += F("function _(el) {return document.getElementById(el);}");
  webpage += F("function uploadFile() {");
  webpage += F("var formdata = new FormData(_('upload'));");
  webpage += F("var ajax = new XMLHttpRequest();");
  webpage += F("ajax.upload.addEventListener(\"progress\", progressHandler, false);");
  webpage += F("ajax.addEventListener(\"loadend\", completeHandler, false);");
  webpage += F("ajax.open('POST', '/fupload',true);");
  webpage += F("ajax.send(formdata);");
  webpage += F("}");

  webpage += F("function progressHandler(event) {");
  webpage += F("_(\"loaded_n_total\").innerHTML = \"Uploaded \" + event.loaded + \" bytes of \" + event.total;");
  webpage += F("var percent = (event.total > 0) ? ((event.loaded / event.total) * 100):0;");
  webpage += F("_(\"progressBar\").value = Math.round(percent); _(\"status\").innerHTML = Math.round(percent) + \"% uploaded... please wait\";}");

  webpage += F("function completeHandler(event) {_(\"status\").innerHTML = \"complete\"; _(\"progressBar\").value = 0;location.reload();}");
 
  //server.setContentLength(CONTENT_LENGTH_UNKNOWN); 
  SendCacheHeaders();
  server.send(200, "text/javascript", webpage); // Empty content inhibits Content-length header so we have to close the socket ourselves. 
  webpage="";
}

void file_style_css()
{
  webpage += F("body{max-width:65%;margin:0 auto;font-family:arial;font-size:105%;text-align:center;color:blue;background-color:#F7F2Fd;}");
  webpage += F("ul{list-style-type:none;margin:0.1em;padding:0;border-radius:0.375em;overflow:hidden;background-color:#dcade6;font-size:1em;}");
  webpage += F("li{float:left;border-radius:0.375em;border-right:0.06em solid #bbb;}last-child {border-right:none;font-size:85%}");
  webpage += F("li a{display: block;border-radius:0.375em;padding:0.44em 0.44em;text-decoration:none;font-size:85%}");
  webpage += F("li a:hover{background-color:#EAE3EA;border-radius:0.375em;font-size:85%}");
  webpage += F("section {font-size:0.88em;}");
  webpage += F("h1{color:white;border-radius:0.5em;font-size:1em;padding:0.2em 0.2em;background:#558ED5;}");
  webpage += F("h2{color:orange;font-size:1.0em;}");
  webpage += F("h3{font-size:0.8em;}");
  webpage += F("table{font-family:arial,sans-serif;font-size:0.9em;border-collapse:collapse;width:85%;}"); 
  webpage += F("th,td {border:0.06em solid #dddddd;text-align:left;padding:0.3em;border-bottom:0.06em solid #dddddd;}"); 
  webpage += F("tr:nth-child(odd) {background-color:#eeeeee;}");
  webpage += F(".rcorners_n {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:20%;color:white;font-size:75%;}");
  webpage += F(".rcorners_m {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:50%;color:white;font-size:75%;}");
  webpage += F(".rcorners_w {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:70%;color:white;font-size:75%;}");
  webpage += F(".column{float:left;width:50%;height:45%;}");
  webpage += F(".row:after{content:'';display:table;clear:both;}");
  webpage += F("button:disabled{color:gray;background-color:lightgray;}");
  webpage += F("*{box-sizing:border-box;}");
//  webpage += F("footer{background-color:#eedfff; text-align:center;padding:0.3em 0.3em;border-radius:0.375em;font-size:60%;}");
  webpage += F("button{border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:20%;color:white;font-size:130%;}");
  webpage += F(".buttons {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:15%;color:white;font-size:80%;}");
//  webpage += F(".buttonsm{border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:9%; color:white;font-size:70%;}");
//  webpage += F(".buttonm {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:15%;color:white;font-size:70%;}");
//  webpage += F(".buttonw {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:40%;color:white;font-size:70%;}");
//  webpage += F("a{font-size:75%;}");
  webpage += F("p{font-size:75%;}");

    SendCacheHeaders();
  server.send(200, "text/css", webpage); // Empty content inhibits Content-length header so we have to close the socket ourselves. 
  webpage="";
}

