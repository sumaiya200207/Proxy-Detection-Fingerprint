#include <Adafruit_Fingerprint.h>
#include <string>
#include <WiFi.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include "FirebaseESP32.h"
#include "Keypad.h"

#define FIREBASE_HOST "attendance-system-6d7a6-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "AIzaSyCI03PF-cz7jkTJxx9lSJt7uy4OSy_mVHM"
#define WIFI_SSID "Sumaiya"
#define WIFI_PASSWORD "sumaiya5500"
#define ROOM 1204

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] =
{
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' }
};

byte rowPins[ROWS] = { 23, 22, 3, 21 };
byte colPins[COLS] = { 19, 18, 5, 17 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

FirebaseData firebaseData;
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);

uint8_t p = -1;
int counter = 1;


void setup()
{
    Serial.begin(9600);
    initWifi();
    lcd.begin();

    while (!Serial);
    delay(100);

    Serial.println("\n\nAdafruit Fingerprint sensor enrollment");
    finger.begin(57600);

    if (finger.verifyPassword())
    {
        Serial.println("Found fingerprint sensor!");
    }
    else
    {
        Serial.println("Did not find fingerprint sensor :(");

        while (1)
        {
            delay(1);
        }
    }

    Serial.println(F("Reading sensor parameters"));
    finger.getParameters();
    Serial.print(F("Status: 0x"));
    Serial.println(finger.status_reg, HEX);
    Serial.print(F("Sys ID: 0x"));
    Serial.println(finger.system_id, HEX);
    Serial.print(F("Capacity: "));
    Serial.println(finger.capacity);
    Serial.print(F("Security level: "));
    Serial.println(finger.security_level);
    Serial.print(F("Device address: "));
    Serial.println(finger.device_addr, HEX);
    Serial.print(F("Packet len: "));
    Serial.println(finger.packet_len);
    Serial.print(F("Baud rate: "));
    Serial.println(finger.baud_rate);
}


void initWifi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }

    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    Firebase.reconnectWiFi(true);
}


void loop()
{
    if (counter == 1)
    {
        displayMsg("Enter an option...");
        Serial.println("Enter an option...");
        displayMsg("'1' -> enroll");
        displayMsg("'2' -> verify");
        displayMsg("'3'->clear database");

    }

    counter = 0;
    char key = keypad.getKey();

    if (key)
    {
        Serial.println(key);
        displayMsgInt(key-'0');

        switch (key)
        {
            case '1':
                enrollFingerprint();
                counter = 1;
                break;

            case '2':
                verifyFingerprint();
                counter = 1;
                break;

            case '3':
                deleteFingerprint();
                counter = 1;
                break;
        }
    }
}
void displayMsg(String msg)
{
    delay(1000);
    lcd.clear();
	  lcd.print(msg);
}

void displayMsgInt(int msgI)
{
    delay(1000);
    lcd.clear();
	  lcd.print(msgI);

}

void deleteFingerprint()
{
    finger.emptyDatabase();
    displayMsg("Database's empty");
}


void enrollFingerprint()
{
    int id = readnumber();
    displayMsg("Place finger...");

    if (readFingerprintSensor() != FINGERPRINT_OK)
    {
        return;
    }

    finger.image2Tz(1);
    displayMsg("Same finger..");

    if (readFingerprintSensor() != FINGERPRINT_OK)
    {
        return;
    }

    finger.image2Tz(2);

    if (createFingerprintTemplate(id) != FINGERPRINT_OK)
    {
        return;
    }

    if (loadFingerprintTemplate(id) != FINGERPRINT_OK)
    {
        return;
    }

    String data = readFingerprintTemplate();
    sendToFirebase(id, data, 1, ROOM);
    delay(15000);
    receiveFromFirebase();
}


void verifyFingerprint()
{
    displayMsg("Put finger on..");
    readFingerprintSensor();
    finger.image2Tz();

    int id = getFingerprintID();
    loadFingerprintTemplate(id);

    String data = readFingerprintTemplate();
    sendToFirebase(id, data, 2, ROOM);
    delay(15000);
    receiveFromFirebase();
}


uint8_t readnumber(void)
{
    displayMsg("Type ID...");
    delay(2000);
    char key2;
    int flag=2;
    uint8_t value = 0;
    static char previousKey = '\0';

    while (flag!=0)
    {
        key2 = keypad.getKey();
        
        if (key2)
        {
            if (key2 != '\0')
            {
                if (previousKey == '\0')
                {
                    previousKey = key2;
                }
                else
                {
                    if (previousKey >= '0' && previousKey <= '9' && key2 >= '0' && key2 <= '9')
                    {
                        value = (previousKey - '0') * 10 + (key2 - '0');
                    }
                    else
                    {
                        value = previousKey - '0';
                    }
                    previousKey = '\0';
                }
                delay(50);
                flag--;
            }
        }
    }
    displayMsg("Entered id ");
    displayMsgInt(value);
    //Serial.print(String(value));
    return value;
}


uint8_t readFingerprintSensor()
{
    p = -1;

    while (p != FINGERPRINT_OK)
    {
        p = finger.getImage();

        switch (p)
        {
            case FINGERPRINT_OK:
                displayMsg("Image taken");
                break;

            case FINGERPRINT_NOFINGER:
                break;

            case FINGERPRINT_PACKETRECIEVEERR:
                Serial.println("Communication error");
                break;

            case FINGERPRINT_IMAGEFAIL:
                Serial.println("Imaging error");
                break;

            default:
                Serial.println("Unknown error");
                break;
        }
    }
    return p;
}


uint8_t createFingerprintTemplate(int id)
{
    finger.createModel();

    if (p == FINGERPRINT_OK)
    {
        displayMsg("Matched!");
    }
    else if (p == FINGERPRINT_PACKETRECIEVEERR)
    {
        Serial.println("Communication error");
        return p;
    }
    else if (p == FINGERPRINT_ENROLLMISMATCH)
    {
        displayMsg("Not matched");
        return p;
    }
    else
    {
        Serial.println("Unknown error");
        return p;
    }

    p = finger.storeModel(id);

    if (p == FINGERPRINT_OK)
    {
        displayMsg("Stored!");
    }
    else if (p == FINGERPRINT_PACKETRECIEVEERR)
    {
        Serial.println("Communication error");
    }
    else if (p == FINGERPRINT_BADLOCATION)
    {
        Serial.println("Could not store in that location");
    }
    else if (p == FINGERPRINT_FLASHERR)
    {
        Serial.println("Error writing to flash");
    }
    else
    {
        Serial.println("Unknown error");
    }

    return p;
}


uint8_t loadFingerprintTemplate(int id)
{
    p = finger.loadModel(id);

    switch (p)
    {
        case FINGERPRINT_OK:
            break;

        case FINGERPRINT_PACKETRECIEVEERR:
            Serial.println("Communication error");
            return p;

        default:
            Serial.print("Unknown error ");
            return p;
    }

    p = finger.getModel();

    switch (p)
    {
        case FINGERPRINT_OK:
            break;
            
        default:
            Serial.print("Unknown error ");
            return p;
        }

    return p;
}


String readFingerprintTemplate()
{
    uint8_t bytesReceived[534];
    memset(bytesReceived, 0xff, 534);
    uint32_t starttime = millis();
    int i = 0;

    while (i < 534 && (millis() - starttime) < 20000)
    {
        if (mySerial.available())
        {
            bytesReceived[i++] = mySerial.read();
        }
    }

    Serial.print(i);
    Serial.println("Bytes read.");
    Serial.println("Decoding packet...");

    uint8_t fingerTemplate[512];
    memset(fingerTemplate, 0xff, 512);
    int uindx = 9, index = 0;
    memcpy(fingerTemplate + index, bytesReceived + uindx, 256);
    uindx += 256;
    uindx += 2;
    uindx += 9;
    index += 256;
    memcpy(fingerTemplate + index, bytesReceived + uindx, 256);

    String result;

    for (int i = 0; i < 512; ++i)
    {
        result += getHex(fingerTemplate[i], 2);
    }

    //Serial.println(result);
    displayMsg("Done.");

    return result;

}


String getHex(int num, int precision)
{
    char tmp[16];
    char format[128];
    sprintf(format, "%%.%dX", precision);
    sprintf(tmp, format, num);

    return String(tmp);
}


void sendToFirebase(int id, String data, int action, int room)
{
    Serial.println(id);
    Firebase.setString(firebaseData, "/Fingerprints/temp/id", String(id));
    Firebase.setString(firebaseData, "/Fingerprints/temp/fingerHex", data);
    Firebase.setString(firebaseData, "/Fingerprints/temp/action", String(action));
    Firebase.setString(firebaseData, "/Fingerprints/temp/room", String(room));
}


void receiveFromFirebase()
{
    if (Firebase.get(firebaseData, "/LatestAudit/latest/log"))
    {
        displayMsg(firebaseData.stringData());
    }
}


int getFingerprintID()
{
    p = finger.fingerSearch();

    if (p == FINGERPRINT_OK)
    {
        displayMsg("Found");
    }
    else if (p == FINGERPRINT_PACKETRECIEVEERR)
    {
        Serial.println("Communication error");
        return p;
    }
    else if (p == FINGERPRINT_NOTFOUND)
    {
        displayMsg("Not found");
        return p;
    }
    else
    {
        Serial.println("Unknown error");
        return p;
    }

    return finger.fingerID;
}
