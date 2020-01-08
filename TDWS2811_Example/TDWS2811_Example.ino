#include "TDWS2811.h"
TDWS2811 td(1000);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Start Setup");
  
  Serial.printf("Setup complete!");
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(2, !digitalRead(2));
  digitalWrite(13, !digitalRead(13));
  td.flipBuffers();
  delay(500);

}
