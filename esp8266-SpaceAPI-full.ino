extern "C" {
  #include "user_interface.h"
}
/*
  2015-07-26: Created by xopr

  This sketch uses the following external library/libraries
  - https://github.com/milesburton/Arduino-Temperature-Control-Library
*/

/*
 * List of defines that you can use to set up a ESP-01
 * DallasTemperature  onewire sensor network
 * DHT                single DHT temp/humidity sensor
 * NeoPixels          neopixel strand
 * SpacestateSwitch   switch input that sets space state
 * 
 * TMP10x             I2C temperature
 * MCP9808            I2C precision temperature
 * MPL115A2           I2C barometric pressure/temperature
 * MPL3115A2          I2C barometric pressure/altitude/temperature
 * LTC2990            I2C voltage/current/temperature
 */
#define DEBUG

//#define DEEP_SLEEP

// Switch and pixels are defined on GPIO0 (choose one)
//#define SPACESTATE        // Space state switch
#define NEOPIXELS         // NeoPixels spacestate indicator
#define NEOPIXEL_COUNT 8  // Number of NeoPixels on the connected string

// 'complex' Sensors are on GPIO2 (choose one)
// NOTE: this pin is pulled to GND to activate the programming bootloader)
//#define DALLAS_TEMPERATURE  // Dallas onewire temperature network
//NOT IMPLEMENTED //#define DHT_SENSOR          // DHT temperature and humidity sensor
//NOT IMPLEMENTED //#define DHT_ID [ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ]

// I2C (two wire interface) needs both GPIO0 and GPIO2
//NOT IMPLEMENTED //#define I2C

// Accesspoint definitions
const char* strSsid     = "<your_accesspoint>";
const char* strPassword = "<your_password>";

// API settings
const char* strServer   = "<your_domain.tld>";
const char* strApiKey   = "<your_api_key>";

///////////////////////////////////////////////////////////////////////////////////
// NOTE: you progably don't need to edit anything below this point, unless you want to break stuff
///////////////////////////////////////////////////////////////////////////////////
// NOTE: GPIO0 is also the bootloader mode pin
#define PIN_NEOPIXELS 0
#define PIN_SWITCH    2
//#define PIN_ONEWIRE   2

// Sanity check on GPIO0
#if defined( SPACESTATE ) && defined( NEOPIXELS )
    #error Cannot use spacestate switch and neopixels: they use the same pin.
#endif
#if defined( NEOPIXELS ) && !defined( NEOPIXEL_COUNT )
    #error You must define the number of NeoPixels if you want to use them (NEOPIXEL_COUNT).
#endif

// Sanity check on GPIO2
#if defined( DALLAS_TEMPERATURE ) && defined( DHT_SENSOR )
    #error Cannot use Dallas temperature and DHT sensor: they use the same pin.
#endif
#if defined( DHT_SENSOR ) && !defined( DHT_ID )
    #error You must define a custom ID if you want to use DHT (DHT_ID).
#endif

// Sanity check on I2C (GPIO0 and GPIO2)
#if defined( I2C ) && ( defined( SPACESTATE ) || defined( NEOPIXELS ) || defined( DALLAS_TEMPERATURE ) || defined( DHT_SENSOR ) )
    #error Cannot combine I2C (Two Wire Interface) with any other sensors or outputs.
#endif

// Used libraries
#include <ESP8266WiFi.h>

// Forward declarations
void wifiConnect();

#ifdef NEOPIXELS
#include "JsonVarFetch.h"
//#include "StreamJsonReader.h"
#include <Adafruit_NeoPixel.h>
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel pixels = Adafruit_NeoPixel( NEOPIXEL_COUNT, PIN_NEOPIXELS, NEO_GRB + NEO_KHZ800);

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.
#endif

// Dallas temperature uses onewire library
#ifdef DALLAS_TEMPERATURE
#define SENSORS
#define ONEWIRE
#endif

#ifdef ONEWIRE
#include <OneWire.h>
OneWire oneWire( PIN_ONEWIRE );
#endif

// TODO: this is where my file got corrupted
//#ifdef DALLAS_TEMPERATURE

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setup
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
    Serial.begin( 115200 );
    delay( 10 );

    // Apparently, we need to explicitly set the NeoPixel pin as output
#ifdef NEOPIXELS
    pinMode( PIN_NEOPIXELS, OUTPUT );
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// loop
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop( )
{
    connectWireless( );

    updateSpacestateIndicator( );

    /*
    WAKE_RF_DEFAULT = 0, // RF_CAL or not after deep-sleep wake up, depends on init data byte 108.
    WAKE_RFCAL = 1,      // RF_CAL after deep-sleep wake up, there will be large current.
    WAKE_NO_RFCAL = 2,   // no RF_CAL after deep-sleep wake up, there will only be small current.
    WAKE_RF_DISABLED = 4 // disable RF after deep-sleep wake up, just like modem sleep, there will be the smallest current.
*/
#ifdef DEEP_SLEEP
    // NOTE: We need an extra sleep after calling deepSleep for it to execute.
    //       This should be esp_yield(). See https://github.com/esp8266/Arduino/issues/609
    ESP.deepSleep( 20000000, WAKE_RF_DEFAULT );
    delay( 1000 );
#else
    delay( 20000 );
#endif

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// connectWireless
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void connectWireless()
{
    if ( WiFi.status( ) == WL_CONNECTED )
        return;

    Serial.print( "\nConnecting to " );
    Serial.print( strSsid );

    WiFi.begin( strSsid, strPassword );

    bool bLed = true;

#ifdef NEOPIXELS
    // NOTE: currently, rtc memory is empty upon wake. See https://github.com/esp8266/Arduino/issues/619
    // Read color from the RTC memory which should preserve our stored color
    uint32_t storedColor;
    system_rtc_mem_read( 64, &storedColor, 4 );
#endif

    while ( WiFi.status( ) != WL_CONNECTED )
    {

#ifdef NEOPIXELS
        for(uint16_t i=0; i<pixels.numPixels(); i++)
            pixels.setPixelColor( i, bLed ? pixels.Color( 0, 255, 128 ) : storedColor );
        pixels.show( );
        bLed = !bLed;
#endif

        delay( 500 );
        Serial.print( "." );
    }

/*
#ifdef NEOPIXELS
        for(uint16_t i=0; i<pixels.numPixels(); i++)
            pixels.setPixelColor( i, pixels.Color( 0, 0, 0 ) );

        pixels.show(); // This sends the updated pixel color to the hardware.
#endif
*/

    Serial.println( "" );
    Serial.print( "connected; IP:" );
    Serial.println( WiFi.localIP( ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// updateSpacestateIndicator
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void updateSpacestateIndicator()
{
#ifdef NEOPIXELS
    uint32_t color;
    color = determineSpaceStateColor();
    //color = generateRandomColor();

    //system_rtc_mem_read( 64, &storedColor, 4 );
    system_rtc_mem_write( 64, &color, 4 );


    for(uint16_t i=0; i<pixels.numPixels(); i++)
        pixels.setPixelColor( i, color );

    pixels.show(); // This sends the updated pixel color to the hardware.

#endif
}

#ifdef NEOPIXELS
uint32_t generateRandomColor()
{
    byte arrColors[] = { 0, 1, 2, 3, 4, 6, 8, 12, 16, 23, 32, 45, 64, 90, 128, 180, 255 };
    return pixels.Color( arrColors[ random( sizeof( arrColors ) ) ], arrColors[ random( sizeof( arrColors ) ) ], arrColors[ random( sizeof( arrColors ) ) ] );
}

uint32_t determineSpaceStateColor()
{
    WiFiClient  client;
    char        character;
    uint8_t     nNewLines = 0;
    uint16_t    nPos = 0;

    //static const char* queries[] = { "state.open" };
    //StreamJsonReader jsonreader( queries, 1 );
    
    static const char* arrQueries[] = { "state.open" };
    char strResult[ 64 ] = { 0 };

    JsonVarFetch jsonVarFetch( arrQueries, 1, strResult, 64 );

    if ( !client.connect( strServer, 80 ) )
    {
        Serial.println("connection failed");
        // Cannot connect, return blue
        return pixels.Color( 0, 0, 255 );
    }

    // We now create a URI for the request
    String strUrl = "/spaceAPI/";

    Serial.print( "Requesting URL: " );
    Serial.println( strUrl );
  
    // This will send the request to the server
    client.print( String("GET ") + strUrl + " HTTP/1.0\r\n" +
                  "Host: " + strServer + "\r\n\r\n" );

    // Read until we have a valid character
    character = client.read( );
    while( character == 255 )
    {
        delay( 10 );
        character = client.read( );
    }

    // Read data
    while( client.connected() && character != 255 /*&& !jsonreader.finished( )*/ )
    {
        switch ( character )
        {
            case '\n':
            case '\r':
                nNewLines++;
                break;

            default:
                if ( nNewLines < 4 )
                    nNewLines = 0;
                break;
        }

        if ( nNewLines >= 4 )
        {
            nPos++;
            //Serial.print( character );
        }

        ParseStatus::Enum eParseStatus;
        if ( nNewLines >= 4 && ( eParseStatus = jsonVarFetch.processCharacter( character ) ) < ParseStatus::Ok )
        {
            switch ( eParseStatus )
            {
                case ParseStatus::Ok:
                    Serial.println( "Ok" );
                    break;
        
                case ParseStatus::AllocationError:
                    Serial.println( "AllocationError" );
                    break;
        
                case ParseStatus::ParserError:
                    Serial.println( "ParserError" );
                    break;
        
                case ParseStatus::JsonError:
                    Serial.println( "JsonError" );
                    break;
        
                case ParseStatus::Complete:
                    Serial.println( "Complete" );
                    break;
        
                case ParseStatus::CompletePartialResult:
                    Serial.println( "CompletePartialResult" );
                    break;
        
                case ParseStatus::CompleteFullResult:
                    Serial.println( "CompleteFullResult" );
                    break;
        
                default:
                    Serial.println( "Unknown parser state, this should not happen!" );
                    break;
            }

            Serial.print( nPos );
            Serial.print( "=>" );
            Serial.print( character );
            Serial.print( " (0x" );
            Serial.print( character, HEX );
            Serial.println( ")" );
            return pixels.Color( 255, 0, 255 );
        }

        character = client.read( );
    }

    // Space open, return green
    if ( strcmpi( strResult, "true" ) == 0 )
    {
        Serial.println( "Open!" );
        return pixels.Color( 0, 255, 0 );
    }

    // Space closed, return red
    if ( strcmpi( strResult, "false" ) == 0 )
    {
        Serial.println( "Closed" );
        return pixels.Color( 255, 0, 0 );
    }

    // Unknown state, return yellow
    Serial.println( "Unknown" );
    return pixels.Color( 255, 150, 0 );
}
#endif
