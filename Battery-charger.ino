/**********************************
*  Chargeur de batteries Lithium
* 
*  Auteur : Eric HANACEK
*     2021-11-18
*     Modification: 2022-01-04
*     
* Ecran : TFT ILI9341, 240x320
* Relais : 8
* INA : 3 x INA3221
* 
***********************************
*/
#include <TFT_eSPI.h> // Graphics and font library for driver chip
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
  float shuntvoltage;
  float busvoltage;
  float current_mA;
  float loadvoltage;
  float power_mW;
  float energy_mAh;
  float Tps;
  int ptCharge;
};
struct stVoie Voies[8];

//Gestion du bouton départ charge
const int pinBouton = 0;
boolean buttonOK = false;

unsigned long currentTime = millis(); // Current time

//Gestion des pages
int pageNum = 0;
int pageNumPrev = 0;

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
  tft.drawString("version : 1.02", 0, 224, 2); //->240-16

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
    tft.print("MAC : "); 
    tft.println(ESP.getEfuseMac(), HEX); 
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
	Voies[voie-1].ptCharge = niveauCB.getChargeLevel_18650( Voies[voie-1].loadvoltage );
  //----- Temps total de charge
  //Voies[voie-1].Tps = Voies[voie-1].Tps + TpsIntSec;
  //if (Voies[voie-1].current_mA > 0) Voies[voie-1].Tps = Voies[voie-1].Tps + TpsIntSec;
  if (Voies[voie-1].current_mA > 0) Voies[voie-1].Tps = Voies[voie-1].Tps + 0.1;
	
	//----- Gestion de la charge en fonction de seuils
  if (current_mA < 2 and buttonOK == false) {
    digitalWrite(pinRelais[voie-1], HIGH);
  }
  else if (loadvoltage >= 3.0 and buttonOK == true) {
    digitalWrite(pinRelais[voie-1], LOW);
  }
  
}

//***********************************************************************
// Affichage des mesures d'une voie
//***********************************************************************
void AfficheVoie(int voie) {

  //affichage des mesures de  la voie
  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.drawNumber(voie, 0, ligne[voie-1], 2);
  
  //Pas de batterie ou batterie HS
  if (Voies[voie-1].loadvoltage<3.0) {
    digitalWrite(pinRelais[voie-1], HIGH); // ouvrir le relais
    tft.setTextColor(TFT_RED, TFT_BLACK);
    //tft.drawString("Pas de batterie ou batterie HS                   ", col1, ligne[voie-1], 2);
    tft.drawString("Batterie inexistante ou profondement dechargee   ", col1, ligne[voie-1], 2);
  }
  //Chargement batterie mise en page '8 par caratères', '6 pour le .'
  else {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("                                                   ", col1, ligne[voie-1], 2);
    //loadvoltage
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
    tft.drawNumber(Voies[voie-1].ptCharge, col3, ligne[voie-1], 2);
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

    //---- Baterie chargée
    if (Voies[voie-1].loadvoltage>4.15 and Voies[voie-1].current_mA<2 and buttonOK == false) {
       digitalWrite(pinRelais[voie-1], HIGH); // ouvrir le relais
      //tft.setTextColor(TFT_GREEN, TFT_BLACK);
      //tft.drawString("Batterie Ok                ", col1, ligne[voie-1], 2);
      //tft.drawString("Batterie complete          ", col1, ligne[voie-1], 2);
      //tft.drawString("Batterie Ok                                      ", col1, ligne[voie-1], 2);
      //tft.drawString("Batterie complète                                ", col1, ligne[voie-1], 2);
      //tft.drawFloat(Voies[voie-1].energy_mAh, 0, col4, ligne[voie-1], 2);
      //tft.drawNumber(Voies[voie-1].energy_mAh, col4, ligne[voie-1], 2);
      //tft.drawString("mAh",col4+45, ligne[voie-1], 2);
    } 
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

}

//***********************************************************************
// Affichage la jauge d'une voie
//***********************************************************************
void AfficheJauge(int voie) {

}

//***********************************************************************
// Affichage les jauges sur une page
//***********************************************************************
void AfficheJauges(void) {
  
  //Affichage des mesures des voies
  for (int i = 1; i <= 8; i++) AfficheJauge(i);

}

//***********************************************************************
// Main loop
//***********************************************************************
void loop() {

  //Prendre les mesures des voies
  currentMillis = millis();
  
  if (currentMillis - MesuresMillis >= 100) {
    //Interval = ((float)currentMillis - (float)MesuresMillis)/1000;
    //Serial.print("interval: "); Serial.println(Interval);
    for (int i = 1; i <= 8; i++) PrendreMesures(i);
    MesuresMillis = currentMillis;
  } 
  
  //Affichage des pages
  if (currentMillis - AffichageMillis >= 1600) {
    AffichageMillis = currentMillis;
    switch (pageNum) {
      case 0:
        if (pageNumPrev != pageNum) { 
          pageNumPrev = pageNum;
          tft.fillScreen(TFT_BLACK);
        }
        AfficheMesures();
        //AfficheJauges();
        break;
      case 1:
        //Afficher une fois donc pas de rafraichissement
        if (pageNumPrev != pageNum) {
          pageNumPrev = pageNum;
          HelloWorld(); //Affiche page démarrage
        }
        break;
      /*case 2:
        if (pageNumPrev != pageNum) {
          pageNumPrev = pageNum; }
          analogMeter(); // Draw analogue Meter
        }
        plotNeedle(443,0); // trace l'aiguille
        //plotNeedle(Voies[0].current_mA,0); // trace l'aiguille
        //Serial.println(millis()-t); // Print time taken for meter update
        break;*/
      /*case 3:
        if (pageNumPrev != pageNum) {
          pageNumPrev = pageNum;
          //plotPointer(); // Draw Linear Meter
        }
        break;*/
    }    
  }
  
  //Etat du bouton
  if (digitalRead(pinBouton)==LOW) {
    buttonOK = true;
    //tft.drawString("ON  ", 0, 224, 2); //->240-16
		pageNum = 1; //HelloWorld();
  }
  else if (buttonOK == true){
    buttonOK = false;
		pageNum = 0; 
    //tft.drawString("OFF ", 0, 224, 2); //->240-16
    // Changement de page
    //pageNum = pageNum + 1;
    //if (pageNum > 2) pageNum = 0;
    ////tft.fillScreen(TFT_BLACK);
  }

}
