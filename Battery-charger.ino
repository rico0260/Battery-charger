/**********************************
*  Chargeur de batteries Lithium
* 
*  Auteur : Eric HANACEK
*     2021-11-18
*     Modification: 2022-10-26
*     
* Microcontroleur : ESP32_Devkitc_V4
* Ecran : TFT ILI9341, 240x320
* Relais : 8
* INA : 3 x INA3221
* 
***********************************
*/
#include <TFT_eSPI.h> // Graphics and font library for driver chip$
#include <SPI.h>

#include <Wire.h>
#include "SDL_Arduino_INA3221.h"

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

setup_t user;

#include "Rico0260_ChargeLevel.h"
#include "includes/GRTgaz_orange_2.h" 

Rico0260_ChargeLevel niveauCB;

SDL_Arduino_INA3221 ina3221_1(0x40);
SDL_Arduino_INA3221 ina3221_2(0x41);
SDL_Arduino_INA3221 ina3221_3(0x42);

// Déclaration et initialisation des GPIO pour les relais
//const int pinRelais[] = {13, 12, 32, 33, 25, 26, 27, 14};
const int pinRelais[] = {14, 27, 26, 25, 33, 32, 12, 13};
const int col1 = 16; //2x8 pixels
const int col2 = 64; //8x8 pixels
//const int col3 = 120; //15x8 pixels //Power 
const int col3 = 128; //16x8 pixels //Pourcentage
const int col4 = 184; //23x8 pixels
const int col5 = 258; //32x8 pixels
const int ligne[] = {20, 36, 52, 68, 84, 100, 116, 132};
// Calculs energie
unsigned long currentMillis;
unsigned long AffichageMillis = 0;
unsigned long MesuresMillis = 0;

//const float TensionMin = 3.2; //tension min
//const float TensionMax = 4.2; //tension max

// structure d'une voie
struct stVoie {
  float shuntvoltage; //Tension sur le shunt
  float busvoltage; //Tension de la batterie
  float current_mA; //Intensité dans le shunt
  float loadvoltage;
  float power_mW; //Puissance utilisée en court
  float energy_mAh; //Energie stockée
  float Tps; //Temps total de charge
  int ptCharge; //Pourcentage de la capacité charge
};
struct stVoie Voies[8];

//Gestion du bouton départ charge
const int pinBouton = 0;
boolean buttonOK = false;
int etat_bouton = 0;

unsigned long currentTime = millis(); // Current time

//Gestion des pages
//int pageNum = 0;
//int pageNumPrev = 0;

//***********************************************************************
// Page Hello de démarrage
//***********************************************************************
void HelloWorld(void) {
unsigned int margeX = 10;
unsigned int posY = 50;

  //tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);

  tft.getSetup(user);
  
  tft.drawXBitmap(margeX, posY-(logoHeight/2), logo, logoWidth, logoHeight, TFT_ORANGE);

  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawCentreString("Chargeur", margeX+logoWidth+margeX+50, 80, 4);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawCentreString("batteries", margeX+logoWidth+margeX+50, 112, 4);
  tft.drawCentreString("lithium", margeX+logoWidth+margeX+50, 144, 4);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  //tft.drawCentreString("IP: " + IP, 20+logoWidth+20+60, 94, 2); 
  tft.drawString("version : 1.05", 0, 224, 2); //->240-16

  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.setCursor(0, 95, 2);
  tft.print("Processor : ");
  if ( user.esp == 0x8266) tft.println("ESP8266");
  if ( user.esp == 0x32)   tft.println("ESP32");
  if ( user.esp == 0x32F)  tft.println("STM32");
  if ( user.esp == 0x2040) tft.println("RP2040");
  if ( user.esp == 0x0000) tft.println("Generic");
  #ifdef ESP8266 //si ESP8266
    tft.print("VCC : ");
    tft.println(ESP.getVcc()); 
  #endif
  #if defined (ESP32) || defined (ESP8266)
    tft.print("CPU : "); 
    tft.print(ESP.getCpuFreqMHz()); 
    tft.println("MHz");
    //
    //tft.print("MAC : "); 
    //tft.println(ESP.getEfuseMac(), HEX); 
  #endif
  if (user.tft_driver != 0xE9D) // For ePaper displays the size is defined in the sketch
  {
    tft.print("Display driver : "); 
    tft.println(user.tft_driver, HEX); 
    tft.print("Width : "); 
    tft.println(user.tft_width); 
    tft.print("Height : "); 
    tft.println(user.tft_height); 
  }
  if (user.serial==1)        { tft.print("Display SPI frequency = "); tft.print(user.tft_spi_freq/10.0); tft.println("MHz");}
  if (user.pin_tch_cs != -1) { tft.print("Touch SPI frequency   = "); tft.print(user.tch_spi_freq/10.0); tft.println("MHz");}  

}

//***********************************************************************
// setup
//***********************************************************************
void setup(void) {

  //Serial.begin(115200);
  //Serial.println("Chargeur de batteries lithium");

  for (int i=0; i<8; i++){
    pinMode(pinRelais[i], OUTPUT);    // Toutes les pins sont commutées en OUT
    digitalWrite(pinRelais[i], HIGH); // Toutes les pins prennent pour valeur HIGH
  }
  pinMode(pinBouton, INPUT_PULLUP);
  
  ina3221_1.begin();
  ina3221_2.begin();
  ina3221_3.begin();
   
  //Gestion de l'écran
  tft.init();
  tft.setSwapBytes(true); // Swap the byte order for pushImage() - corrects endianness
  tft.setRotation(1);
  
  HelloWorld();
  delay(3000);
  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);

}

//***********************************************************************
// Prise des mesures d'une voie
//***********************************************************************
void PrendreMesures(int voie) {
float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
//float TpsIntSec = 0; //Temps ecoulé depuis la dernière mesure
//float TensionCalculee = 0;

  //TpsIntSec = (float)(currentMillis - MesuresMillis) / 1000;
  
  switch (voie) {
    case 1:
    case 2 ... 3:  
      busvoltage = ina3221_1.getBusVoltage_V(voie);
      shuntvoltage = ina3221_1.getShuntVoltage_mV(voie);
      current_mA = -ina3221_1.getCurrent_mA(voie);  // minus is to get the "sense" right.   - means the battery is charging, + that it is discharging
      loadvoltage = busvoltage + (shuntvoltage / 1000);
      break;
    case 4 ... 6:
      busvoltage = ina3221_2.getBusVoltage_V(voie-3);
      shuntvoltage = ina3221_2.getShuntVoltage_mV(voie-3);
      current_mA = -ina3221_2.getCurrent_mA(voie-3);  // minus is to get the "sense" right.   - means the battery is charging, + that it is discharging
      loadvoltage = busvoltage + (shuntvoltage / 1000);
      break;
    case 7:
    case 8:
      busvoltage = ina3221_3.getBusVoltage_V(voie-6);
      shuntvoltage = ina3221_3.getShuntVoltage_mV(voie-6);
      current_mA = -ina3221_3.getCurrent_mA(voie-6);  // minus is to get the "sense" right.   - means the battery is charging, + that it is discharging
      loadvoltage = busvoltage + (shuntvoltage / 1000);
      break;
  }
  
  //----- Tension de la batterie
  Voies[voie-1].busvoltage = busvoltage;
  //---- Shunt
  Voies[voie-1].shuntvoltage = shuntvoltage;
  Voies[voie-1].current_mA = current_mA;
  Voies[voie-1].loadvoltage = loadvoltage;
  //----- Power
  Voies[voie-1].power_mW = Voies[voie-1].loadvoltage * Voies[voie-1].current_mA;
  if (Voies[voie-1].power_mW < 0) Voies[voie-1].power_mW = 0;
  //Voies[voie-1].energy_mAh = Voies[voie-1].energy_mAh + Voies[voie-1].current_mA * (TpsIntSec / 3600);
  Voies[voie-1].energy_mAh = Voies[voie-1].energy_mAh + Voies[voie-1].current_mA / 36000;
  //----- Pourcentage de la capacité charge (Calcul du pourcentage de charge)
	if (current_mA >0) {
    //TensionCalculee = Voies[i-1].loadvoltage*(current_mA*1000)
    Voies[voie-1].ptCharge = niveauCB.getChargeLevel_18650( Voies[voie-1].busvoltage );
  }
  else {
    Voies[voie-1].ptCharge = niveauCB.getChargeLevel_18650( Voies[voie-1].loadvoltage );
  }
  //----- Temps total de charge
  //Voies[voie-1].Tps = Voies[voie-1].Tps + TpsIntSec;
  //if (Voies[voie-1].current_mA > 0) Voies[voie-1].Tps = Voies[voie-1].Tps + TpsIntSec;
  if (Voies[voie-1].current_mA > 0) Voies[voie-1].Tps = Voies[voie-1].Tps + 0.1;
	  
}

//***********************************************************************
// Affichage des mesures d'une voie
//***********************************************************************
void AfficheVoie(int voie) {

  //affichage des mesures de  la voie
  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.drawNumber(voie, 0, ligne[voie-1], 2);
  
  //Pas de batterie ou batterie HS
  if (Voies[voie-1].loadvoltage<2.9) {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    //tft.drawString("Pas de batterie ou batterie HS                   ", col1, ligne[voie-1], 2);
    tft.drawString("Batterie inexistante ou profondement dechargee   ", col1, ligne[voie-1], 2);
  }
  //Chargement batterie mise en page '8 par caratères', '6 pour le .'
  else {

    //Coloration du texte 
    if (Voies[voie-1].current_mA>0) {
      //Batterie en charge
      tft.setTextColor(TFT_RED, TFT_BLACK);
    }
    else {
      //if (Voies[voie-1].loadvoltage>4.05) {
      if (niveauCB.getChargeLevel_18650( Voies[voie-1].busvoltage ) > 80) {
        //batterie chargée
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
      }
      else if (Voies[voie-1].loadvoltage<0.4) {
        //Pas de batterie ou problème batterie 
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("Batterie inexistante ou profondement dechargee   ", col1, ligne[voie-1], 2);
      }
      //else if (Voies[voie-1].loadvoltage<3.4) {
      else if (niveauCB.getChargeLevel_18650( Voies[voie-1].busvoltage ) < 20) {
        //problème batterie
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      }
      else {
        //Batterie non en charge
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      }
    }

    tft.drawString("                                                   ", col1, ligne[voie-1], 2);
    //---- loadvoltage
    tft.drawFloat(Voies[voie-1].loadvoltage, 2, col1, ligne[voie-1], 2);
    tft.drawChar('V', col1+30, ligne[voie-1], 2);
    //---- current_mA
    //tft.drawString(dtostrf(loadvoltage,2,2,tmp), 100, 200, 6);
    //tft.drawFloat(Voies[voie-1].current_mA, 0, col2, ligne[voie-1], 2);
    //tft.drawString("mA",col2+45, ligne[voie-1], 2);
    //tft.drawString("9999", col2, ligne[voie-1], 2);
    tft.drawNumber(Voies[voie-1].current_mA, col2, ligne[voie-1], 2);
    tft.drawString("mA",col2+32, ligne[voie-1], 2);
    //---- power_mW 
    //tft.drawFloat(Voies[voie-1].power_mW, 1, col3, ligne[voie-1], 2);
    //tft.drawString("9999", col3, ligne[voie-1], 2);
    //tft.drawNumber(Voies[voie-1].power_mW, col3, ligne[voie-1], 2);
    //tft.drawString("mW",col3+32, ligne[voie-1], 2);
    //---- pourcentage %
    if (Voies[voie-1].current_mA>0) {
      //en charge
      tft.drawString("xxx",col3, ligne[voie-1], 2);
    }
    else {
      tft.drawNumber(Voies[voie-1].ptCharge, col3, ligne[voie-1], 2);  
    }
    tft.drawString("%",col3+24, ligne[voie-1], 2);
    //---- energy_mAh
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    //tft.drawFloat(Voies[voie-1].energy_mAh, 0, col4, ligne[voie-1], 2);
    //tft.drawString("99999", col4, ligne[voie-1], 2);
    tft.drawNumber(Voies[voie-1].energy_mAh, col4, ligne[voie-1], 2);
    tft.drawString("mAh",col4+40, ligne[voie-1], 2);
    //---- Tps (minutes)
    //tft.setTextColor(TFT_BLUE, TFT_BLACK);
    //tft.drawString("9999", col5, ligne[voie-1], 2);
    tft.drawNumber(Voies[voie-1].Tps/60, col5, ligne[voie-1], 2);
    tft.drawString("min",col5+40, ligne[voie-1], 2);

    //---- busvoltage
    //tft.drawFloat(Voies[voie-1].busvoltage, 2, col5, ligne[voie-1], 2);
    //tft.drawString("V",col5+40, ligne[voie-1], 2);
  }
  
}

//***********************************************************************
// Affichage des mesures sur une page
//***********************************************************************
void AfficheMesures(void) {
  
  //tft.setTextSize(1);
  //tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.drawString("Ubat", col1, 0, 2);
  //tft.drawString("I", col2, 0, 2);
  tft.drawString("U (V)", col1, 0, 2);
  tft.drawString("I (mA)", col2, 0, 2);
  //tft.drawString("P (mW)", col3, 0, 2);
  tft.drawString("(%)", col3, 0, 2);
  tft.drawString("(mAh)", col4, 0, 2);
  tft.drawString("T (min)", col5, 0, 2);

  //Affichage des mesures des voies
  for (int i = 1; i <= 8; i++) AfficheVoie(i);

  //Information
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  //tft.drawString("N'oubliez pas de lancer le processus de charge", 10, 150, 2);
  tft.drawCentreString("N'oubliez pas de lancer le processus de charge", 160, 170, 2);
  //tft.drawString("En charge: Rouge", 20, 190, 2);
  //tft.drawString("Charge OK: Vert", 20, 210, 2);
  tft.setCursor(20, 190, 2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.print("En charge : ");
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.println("Rouge");
  tft.setCursor(20, 210, 2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.print("Charge OK : ");
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("Vert");
  
}

//***********************************************************************
// Main loop
//***********************************************************************
void loop() {

  //Etat du bouton
  if (digitalRead(pinBouton)==LOW) {
    buttonOK = true;
    etat_bouton = 0;
    if (etat_bouton==0) {
      etat_bouton = 1;
      tft.drawString("ON", 300, 224, 2); //->240-16 | 320-5x4
      //pageNum = 1; //HelloWorld();
    }
  }
  else if (buttonOK == true){
    buttonOK = false;
    etat_bouton = 0;
    tft.drawString("   ", 300, 224, 2); //->240-16
    //pageNum = 0; 
    // Changement de page
    //pageNum = pageNum + 1;
    //if (pageNum > 2) pageNum = 0;
    ////tft.fillScreen(TFT_BLACK);
  }

  //Prendre les mesures des voies
  currentMillis = millis();
  
  if (currentMillis - MesuresMillis >= 100) {
    MesuresMillis = currentMillis;
    for (int i = 1; i <= 8; i++) {
      PrendreMesures(i);
      //----- Gestion de la charge en fonction de seuils
      if (buttonOK == false) {
        //---- Baterie chargée
        if ((Voies[i-1].current_mA < 5) or (Voies[i-1].loadvoltage<2.9)) {
          digitalWrite(pinRelais[i-1], HIGH); // ouvrir le relais
        }
      }
      else {
        if (Voies[i-1].loadvoltage >= 2.9) {
          if (Voies[i-1].current_mA < 5) {
            Voies[i-1].energy_mAh = 0.0;
            Voies[i-1].Tps = 0.0;
            Voies[i-1].Tps = 0;
          }
          digitalWrite(pinRelais[i-1], LOW);
        }
      }
    } //for
  } 
  
  //Affichage des pages
  if (currentMillis - AffichageMillis >= 1600) {
    AffichageMillis = currentMillis;
    AfficheMesures();
  }
    
}
