/*
 * --------------------------------------------------------------------------------------------------------------------
 * Demo program za citanje RFID kartica za naplatu pranja kola
 * 
 * Arduino Nano atmega168pa ima 512 B EEPROM memorije (cuvanje podataka od oko 100 korisnika, 5B/korisnik)
 * 
 * Izgled memorije u EEPROMU
 * --------------------------------
 * Adresa    tip podatka
 * --------------------------------
 * 0         item count         | broj UID-eva u EEPROMU
 * -------------------------------
 * 1         prvi byte UID      | PRVI
 * 2         drugi byte UID     | UID
 * 3         treci byte UID     | U
 * 4         cetvrti byte UID   | EEPROMU
 * 5         broj pranja (0-255)
 * -------------------------------
 * 6         prvi byte UID      | DRUGI
 * 7         drugi byte UID     | UID
 * 8         treci byte UID     | U
 * 9         cetvrti byte UID   | EEPROMU
 * 10        broj pranja (0-255)
 * -------------------------------
 * ...
 * 
 * --------------------------------------------------------------------------------------------------------------------
 * 
 * Datum      Opis nadogradnje
 * -------------------------------
 * 19.3.25    LCD dispej
 * 20.3.25    Zujalica
 * 20.3.25    Doplata kredita 
 * 21.3.25    Efikasnija pretraga i upis u EEPROM
 * 
 * --------------------------------------------------------------------------------------------------------------------
 * Made by Filikapec
 */

#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

uint8_t clock[8] = {0x0, 0xe, 0x15, 0x17, 0x11, 0xe, 0x0};

#define SS_PIN 10
#define RST_PIN 9

//#define MID_BTN 6
#define OK_BTN 3 //levo dugme
#define ADD_BTN 5 //desno dugme

#define BUZZ 4

#define NEAKTIVNOST_S 8

#define VREDNOST_DOPLATE 10

#define STANJE_MAX 100;
 
MFRC522 rfid(SS_PIN, RST_PIN); // Instanca klase MFRC522
LiquidCrystal_I2C lcd(0x27, 16, 2); //Instanca klase LCD I2C

// Niz u kom cuvamo UID
byte UID[4];

int itemCount;

byte doplata=0;

//za biranje opcija
bool imaSredstava = false;

bool screenSaver = false;

bool ucitanaKartica = false;

bool doplataUToku = false;

bool naplataUToku = false;

int procitaniIndex=0;

unsigned long poslednjaInterakcija = 0; //u slucaju da neko ostavi program na pola da se samo vrati na pocetno

void setup() {
   
  Serial.begin(9600);
  SPI.begin(); // Inicijalizacija SPI
  rfid.PCD_Init(); // Inicijalizacija MFRC522

  Serial.println(F("This code scan the MIFARE Classsic NUID."));
  
  itemCount = EEPROM.read(0); // cita item count sa adrese 0x00

  lcd.begin();
  lcd.print("Dobrodosli!");
  lcd.setCursor(0,1);
  lcd.print("Prislonite TAG");
  ucitanaKartica = false;

  pinMode(ADD_BTN, INPUT);
  pinMode(OK_BTN, INPUT);
  pinMode(BUZZ, OUTPUT);
  digitalWrite(BUZZ, LOW);
  procitaniIndex = 0;
  
}
 
void loop() {

  if(digitalRead(ADD_BTN)==0) {
    poslednjaInterakcija = millis();
    doplata+=VREDNOST_DOPLATE;
    if(doplata > 100) {
      doplata -= 100;
    }
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Vrednost uplate:");
    lcd.setCursor(0,1);
    lcd.print(doplata);
    lcd.setCursor(4,1);
    lcd.print("kredita");
    doplataUToku = true;
    screenSaver = true;
  }

  if(millis() - poslednjaInterakcija > NEAKTIVNOST_S * 1000 && screenSaver==true) {
    lcd.clear();
    lcd.print("Dobrodosli!");
    lcd.setCursor(0,1);
    lcd.print("Prislonite TAG");
    screenSaver = false;
    ucitanaKartica = false;
    doplataUToku = false;
  }

  if(digitalRead(OK_BTN)==0) {
    poslednjaInterakcija = millis();
    screenSaver =true;
    if(ucitanaKartica == false) {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Ocitajte karticu");
      poslednjaInterakcija = millis()-NEAKTIVNOST_S/2;
      return;
    }
    if(imaSredstava==true && ucitanaKartica==true) {
      lcd.clear();
      lcd.setCursor(2,0);
      lcd.print("Pocinje");
      lcd.setCursor(4,1);
      lcd.print("pranje :)");
      //necemo u ovoj liniji da skidamo kredite nego u nekoj nizoj
      naplataUToku = true;
    } else {
      lcd.clear();
      lcd.setCursor(3,0);
      lcd.print("Nedovoljno");
      lcd.setCursor(4,1);
      lcd.print("sredstava");
      delay(2000);
      lcd.clear();
      lcd.setCursor(3,0);
      lcd.print("Uplatite na");
      lcd.setCursor(4,1);
      lcd.print("terminalu");
    }

    delay(2000);
    lcd.clear();
    lcd.print("Dobrodosli!");
    lcd.setCursor(0,1);
    lcd.print("Prislonite TAG");
    ucitanaKartica = false;
    doplataUToku = false;
  }

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! rfid.PICC_IsNewCardPresent())
    return;
  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  //Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  //Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

    peep();

    
    Serial.println(F("A card has been detected."));

    // Store NUID into UID array
    for (byte i = 0; i < 4; i++) {
      UID[i] = rfid.uid.uidByte[i];
    }

    procitaniIndex = getIndexByID(UID);

    if(procitaniIndex == 0) {
      poslednjaInterakcija = millis()-NEAKTIVNOST_S/2;
      lcd.clear();
      lcd.setCursor(3,0);
      lcd.print("Kartica ne");
      lcd.setCursor(5,1);
      lcd.print("postoji");
      ucitanaKartica = false;
      screenSaver = true;
      doplataUToku = false;
      doplata = 0;
      return;
    }
   
    Serial.print(F("The UID tag is: "));
    
    for(int i=0; i<4; i++) {
      Serial.print(UID[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    Serial.print("Na stanju imate ");
    byte brojPranja = getValByIndex(procitaniIndex);
    Serial.print(brojPranja, HEX);
    Serial.println(" pranja");

    if(naplataUToku == true && brojPranja>0) {
      brojPranja-=1;
      Doplati(procitaniIndex, brojPranja);
      naplataUToku = false;
    }

    
    if(doplataUToku == true && doplata>0) {
      brojPranja+=doplata;
      if(brojPranja > 100) {
        brojPranja-=100;
      }
      Doplati(procitaniIndex, brojPranja);
      doplataUToku=false;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Uplaceno ");
      lcd.print(doplata);
      lcd.setCursor(0,1);
      lcd.print("kredita");
      doplata=0;
      delay(1000);
    }


    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Trenutno stanje:");
    lcd.setCursor(0,1);
    lcd.print(brojPranja);
    lcd.print(" kredita");
    lcd.setCursor(14,1);

    if(brojPranja > 0) {
      lcd.print(">");
      imaSredstava = true;
    } else {
        lcd.print("x");
        imaSredstava = false;
    }

    ucitanaKartica = true;

    poslednjaInterakcija = millis();
    screenSaver = true;
    
    

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

//funkcija koja vraca poziciju UID-a u memoriji, mora postojati barem jedan element, vraca 0 ako takav element ne postoji
byte getIndexByID(byte id[4]) {

  for(int i=1; i<2+(itemCount-1)*5 ; i+=5) {
    if(id[0]==EEPROM.read(i)) {
      if(id[1] == EEPROM.read(i+1)) {
        if(id[2] == EEPROM.read(i+2)) {
          if(id[3] == EEPROM.read(i+3)) {
            return i;
          }
        }
      }
    }
  }
  return 0;
}

/*byte getValByID(byte id[4]) {
  
  for(int i=1; i<2+(itemCount-1)*5 ; i++) {
    if(id[0]==EEPROM.read(i) && id[1] == EEPROM.read(i+1) && id[2] == EEPROM.read(i+2) && id[3] == EEPROM.read(i+3)) {
      return EEPROM.read(i+4);
    }
  }
  
}*/

byte getValByIndex(int index) {
  return EEPROM.read(index+4);
}

byte Doplati(int index, byte novaVrednost) { // ili skini, koristi se za oba
  
  EEPROM.write(index+4, novaVrednost);
}

void peep() {
  digitalWrite(BUZZ, HIGH);
  delay(200);
  digitalWrite(BUZZ, LOW);
}
