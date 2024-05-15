#include <ESP8266WiFi.h>
#include <Servo.h>
#include "LiquidCrystal_I2C.h"
#include <Wire.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <string.h>

#define Addr 0x1C

// CONECTIVIDAD //
const char* ssid = "oneplus 8";
const char* pass = "k84wggeb";
WiFiClient cliente;
HTTPClient http;

// SERVOMOTOR //
Servo servoMotor;
const int pinServo = 12; 	// D6

// ULTRASONIDO //
float duracion, distancia;
float distanciaAnterior = 5.0;
const int pinEcho = 13; 		// D7
const int pinTrigger = 15; 	// D8

// ZUMBADOR //
const int ALARMA = 261;
const int pinZumbador = 5; 	// D0

// ACELEROMETRO //
int xAcc, yAcc, zAcc;
int data[7];

// BOTON //
const int pinBoton = 0;		// D3
unsigned long tiempoDesdePulsacion = 0;
unsigned long delayPulsacion = 1000;
void ICACHE_RAM_ATTR interrupcionBoton();

// CONTROL PUERTA //
int estadoPuerta = 0;
bool estadoAlarma = false;

void avisarIntento() {
	if (WiFi.status() == WL_CONNECTED) {
		http.begin(cliente, "http://192.168.104.190/act_sospechosa");
		http.addHeader("Content-Type", "application/json");
		int respuesta = http.POST("{\"passphrase\":\"c3c0ee1dc1fd04a053fa255f484837df94004a7a\"}");
		if (respuesta <= 0) {
			Serial.println("Error al realizar el POST en avisarIntento");
		} else {
			if (respuesta == HTTP_CODE_OK) {
				const String& msg = http.getString();
        Serial.println(msg);
				if (msg.equals("X")) {
					Serial.println("Se activan las alarmas por intento de acceso no permitido");
					estadoAlarma = true;
					tone(pinZumbador, ALARMA);
					
					// Forzar el cierre
					estadoPuerta = 0;
					servoMotor.write(180 * estadoPuerta);
				} else {
					Serial.println("Actividad permitida");
				}
			} else {
				const String& error = http.getString();
				Serial.println(error);
			}
		}
		http.end();
	}
}

void avisarApertura() {
	if (WiFi.status() == WL_CONNECTED) {
		http.begin(cliente, "http://192.168.104.190/apertura");
		http.addHeader("Content-Type", "application/json");
		int respuesta = http.POST("{\"passphrase\":\"c3c0ee1dc1fd04a053fa255f484837df94004a7a\"}");
		if (respuesta <= 0) {
			Serial.println("Error al realizar el POST en avisarApertura");
		} else {
			if (respuesta == HTTP_CODE_OK) {
				const String& msg = http.getString();
				if (msg.equals("X")) {
					Serial.println("Se activan las alarmas por acceso no permitido");
					estadoAlarma = true;
					tone(pinZumbador, ALARMA);
					
					// Forzar el cierre
					estadoPuerta = 0;
					servoMotor.write(180 * estadoPuerta);
				} else {
					Serial.println("Apertura permitida");
				}
			} else {
				const String& error = http.getString();
				Serial.println(error);
			}
		}
		http.end();
	}
}


void avisarCierre() {
	if (WiFi.status() == WL_CONNECTED) {
		http.begin(cliente, "http://192.168.104.190/cierre");
		http.addHeader("Content-Type", "application/json");
		int respuesta = http.POST("{\"passphrase\":\"c3c0ee1dc1fd04a053fa255f484837df94004a7a\"}");
		if (respuesta <= 0) {
			Serial.println("Error al realizar el POST en avisarApertura");
		} else {
			if (respuesta != HTTP_CODE_OK) {
				const String& error = http.getString();
				Serial.println(error);
			}
		}
		http.end();
	}
}

void interrupcionBoton() {
	unsigned long currentMillis = millis();
	if (currentMillis - tiempoDesdePulsacion >= delayPulsacion) {
		tiempoDesdePulsacion = currentMillis;
		if (digitalRead(pinBoton) == LOW) {
			Serial.println("El boton ha sido pulsado");
			estadoPuerta = (estadoPuerta + 1) % 2;
			servoMotor.write(180 * estadoPuerta);
		}
	}
}

void ultrasonido() {
	digitalWrite(pinTrigger, LOW); digitalWrite(pinEcho, LOW);
	delayMicroseconds(2);

	digitalWrite(pinTrigger, HIGH);
	delayMicroseconds(20);
	
	digitalWrite(pinTrigger, LOW);
	duracion = pulseIn(pinEcho, HIGH);
	distancia = (duracion/ 2.0) * 0.0343;
  // Serial.print("US: ");
  // Serial.println(distancia);
	if (abs(distanciaAnterior - distancia) > 3) { // Se ha producido un cambio en la puerta
		distanciaAnterior = distancia;
		if (distancia > 5) {
			avisarApertura();
		} else {
			avisarCierre(); // Si esto no se pone acaba dando problemas si se da permiso mientras la puerta sigue abierta
		}
	}
}

void acelerometro() {
	// Lectura de 7 bytes, donde cada bit es: 
	// status, xAccl lsb, xAccl msb, yAccl lsb, yAccl msb, zAccl lsb, zAccl msb
	Wire.requestFrom(Addr, 7);
	if (Wire.available() == 7) {
		for (int i = 0; i < 7; i++)
			data[i] = Wire.read();
		
		int x = ((data[1] * 256) + data[2]) / 16;
		if (xAcc > 2047) xAcc -= 4096;
		
		int y = ((data[3] * 256) + data[4]) / 16;
		if (yAcc > 2047) xAcc -= 4096;
		
		int z = ((data[5] * 256) + data[6]) / 16;
		if (zAcc > 2047) xAcc -= 4096;
		
		if (abs(x - xAcc) > 0 || abs(y - yAcc) > 0 || abs(z - zAcc) > 0) {
			avisarIntento();
		}
	}
}

void setup() {
	// SERIAL //
	Serial.begin(9600);
	
	// CONECTIVIDAD //
	WiFi.begin(ssid, pass);
	Serial.print("Estableciendo conexion ");
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(500);
	}
	Serial.print("\n IP: ");
	Serial.println(WiFi.localIP());
	
	// BOTON //
	pinMode(pinBoton, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(pinBoton), interrupcionBoton, FALLING);
	
	// ULTRASONIDO //
	pinMode(pinTrigger, OUTPUT);
	
	// SERVOMOTOR //
	servoMotor.attach(pinServo);
	servoMotor.write(0);
	
	// ACELEROMETRO //
	Wire.begin();
	
	Wire.beginTransmission(Addr);
	Wire.write(0x2A); Wire.write((byte)0x00);
	Wire.endTransmission();
	
	Wire.beginTransmission(Addr);
	Wire.write(0x2A); Wire.write(0x01);
	Wire.endTransmission();
	
	Wire.beginTransmission(Addr);
	Wire.write(0x0E); Wire.write((byte)0x00);
	Wire.endTransmission();
}

void loop() {
	if (estadoAlarma) { // Preguntar si se ha ingresado contraseña para detener estado de alarma
		delay(5000);
		if (WiFi.status() == WL_CONNECTED) {
			http.begin(cliente, "http://192.168.104.190/permiso");
			http.addHeader("Content-Type", "application/json");
			int respuesta = http.POST("{\"passphrase\":\"c3c0ee1dc1fd04a053fa255f484837df94004a7a\"}");
			if (respuesta <= 0) {
				Serial.println("Error al realizar el POST al preguntar por el permiso");
			} else {
				if (respuesta == HTTP_CODE_OK) {
					const String& msg = http.getString();
					if (msg.equals("O")) {
						estadoAlarma = false;
						noTone(pinZumbador);
						Serial.println("Se desactivan las alarmas");
					}
				}
			}
			http.end();
		}
	} else {
		ultrasonido();
		acelerometro();
	}
}