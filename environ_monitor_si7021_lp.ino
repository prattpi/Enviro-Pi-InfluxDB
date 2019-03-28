#include "Adafruit_BluefruitLE_SPI.h"
#include "SparkFun_Si7021_Breakout_Library.h"
#include "Wire.h"
#include "LowPower.h"

float humidity = 0;
float tempf = 0;
int power = A3;
int GND = A2;

#define BUFSIZE                        128   // Size of the read buffer for incoming data
#define VERBOSE_MODE                   true  // If set to 'true' enables debug output
#define BLUEFRUIT_SPI_CS               8
#define BLUEFRUIT_SPI_IRQ              7
#define BLUEFRUIT_SPI_RST              4
#define VBATPIN A9

#if SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif

// Create the bluefruit object
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// Create temp sensor object
Weather sensor;

// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

/* The service information */

int32_t esServiceId;
int32_t tempMeasureCharId;
int32_t humMeasureCharId;
int32_t btServiceId;
int32_t btMeasureCharId;

/**************************************************************************/
/*!
    @brief  Sets up the HW an the BLE module (this function is called
            automatically on startup)
*/
/**************************************************************************/
void setup(void)
{
  boolean success;

  //while (!Serial); // uncomment to test via Serial output
  delay(500);
  
  Serial.begin(115200);
  Serial.println(F("Starting Environmental Monitor"));
  Serial.println(F("---------------------------------------------------"));

  randomSeed(micros());
  
  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit"));
  }
  Serial.println( F("OK!") );

  /* Perform a factory reset to make sure everything is in a known state */
  Serial.println(F("Performing a factory reset: "));
  if (! ble.factoryReset() ){
       error(F("Couldn't do factory reset"));
  }

  /* Disable command echo from Bluefruit */
 // ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  /* Change the device name to make it easier to find */
  Serial.println(F("Setting device name to 'Bluefruit Environmental Sensor': "));

  if (! ble.sendCommandCheckOK(F("AT+GAPDEVNAME=Bluefruit ES")) ) {
    error(F("Could not set device name"));
  }

  /* Add the Environmental Monitor service */
  // Environmental Sensing org.bluetooth.service.environmental_sensing 0x181A  GSS
  
  Serial.println(F("Adding the Environmental Sensing Service definition (UUID = 0x181A): "));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x181A"), &esServiceId);
  if (! success) {
    error(F("Could not add ES service"));
  }

  // Temperature ESS Characteristic
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.temperature.xml
  Serial.println(F("Adding the Temperature Measurement characteristic (UUID = 0x2A6E): "));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2A6E, PROPERTIES=0x10, MIN_LEN=1, MAX_LEN=5"), &tempMeasureCharId);
    if (! success) {
    error(F("Could not add temperature characteristic"));
  }
  
  // Humidity ESS Characteristic
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.humidity.xml
  Serial.println(F("Adding the Humidity Measurement characteristic (UUID = 0x2A6F): "));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2A6F, PROPERTIES=0x10, MIN_LEN=1, MAX_LEN=5"), &humMeasureCharId);
    if (! success) {
    error(F("Could not add humidity characteristic"));
  }

  /* Add the Battery service */
  Serial.println(F("Adding the Battery Service definition (UUID = 0x180F): "));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x180F"), &btServiceId);
  if (! success) {
    error(F("Could not add battery service"));
  }

  // Add a battery measurement characteristic
  Serial.println(F("Adding the battery measurement characteristic (UUID = 0x2A19): "));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2A19,PROPERTIES=0x10,MIN_LEN=1, MAX_LEN=4"), &btMeasureCharId);
  if (! success) {
    error(F("Could not add humidity characteristic"));
  }  

  /* Reset the device for the new service setting changes to take effect */
  Serial.print(F("Performing a SW reset (service changes require a reset): "));
  ble.reset();

  //Initialize the I2C sensors and ping them
  pinMode(GND, OUTPUT);

  digitalWrite(power, HIGH);
  digitalWrite(GND, LOW);

  //Initialize the I2C sensors and ping them
  sensor.begin();

  // Prepare to turn off LED
  pinMode(13, OUTPUT);

  // TRY TO SEE LED MODE
  if (! ble.sendCommandCheckOK(F("AT+HWMODELED=DISABLE")) ) {
    error(F("Could not get LED mode"));
  }

  if (! ble.sendCommandCheckOK(F("AT+HELP")) ) {
    error(F("Could not get commands available"));
  }

  
  if (! ble.sendCommandCheckOK(F("AT+BLEPOWERLEVEL")) ) {
    error(F("Could not get current power level!"));
  }

  delay(1000);
  // Was at 0 by default, could go up a bit too: -12 -8 -4 0
  if (! ble.sendCommandCheckOK(F("AT+BLEPOWERLEVEL=-8")) ) {
    error(F("Could not set power level!"));
  }
  
  Serial.println();
}

/** Continuously send temp/humidity/battery data **/
void loop(void)
{
  // Turn off LED to save battery
  digitalWrite(13, LOW);
  
 // read humidity and temp 
  humidity = sensor.getRH();

  // Measure Temperature
  tempf = sensor.getTempF();
  
  Serial.print(F("Updating tempf value to "));
  Serial.print(tempf);
  Serial.println(F(" degrees"));

  /* AT+GATTCHAR=CharacteristicID,value */
  ble.print( F("AT+GATTCHAR=") );
  ble.print( tempMeasureCharId );
  ble.print( F(",") );
  ble.println( tempf );

  /* Check if command executed OK */
  if ( !ble.waitForOK() )
  {
    Serial.println(F("Failed to get response for temp update!"));
  }

  Serial.print(F("Updating humidity value to "));
  Serial.print(humidity);
  Serial.println(F(" percent relative humidity"));

  /* AT+GATTCHAR=CharacteristicID,value */
  ble.print( F("AT+GATTCHAR=") );
  ble.print( humMeasureCharId );
  ble.print( F(",") );
  ble.println( humidity );

  /* Check if command executed OK */
  if ( !ble.waitForOK() )
  {
    Serial.println(F("Failed to get response for humidity update!"));
  }


  /* Report battery level to service */
  float battery_v = getBatteryLevel();

  Serial.print(F("Updating battery value to "));
  Serial.print(battery_v);
  Serial.println(F("V"));

  /* AT+GATTCHAR=CharacteristicID,value */
  ble.print( F("AT+GATTCHAR=") );
  ble.print( btMeasureCharId );
  ble.print( F(",") );
  ble.println( battery_v );

  /* Check if command executed OK */
  if ( !ble.waitForOK() )
  {
    Serial.println(F("Failed to get response for battery update!"));
  }
  
  /* Delay before next measurement update */

  Serial.println("Powering down between readings...");
  delay(1000);

  // Sleep for about 10 minutes between readings
  // This will kill serial output but device still functions
  int i;
  for (i = 0; i < 66; i++)
  {  
   // Enter ATmega32U4 power down state for 8 s (max without hardware interrupt)
   LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
   delay(1000);
  }  
}

/* Report battery level */
float getBatteryLevel() {
  // Lipoly batteries are 'maxed out' at 4.2V and stick around 3.7V for much of the battery life
  // Bluefruit tends to die around 3.53V
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  Serial.print("VBat: " ); Serial.println(measuredvbat);
  return measuredvbat;
}
