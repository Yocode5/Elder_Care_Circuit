const int FSR_PIN = 34;

void setup(){
    Serial.begin(115200);

}

void loop(){
    int fsrValue = analogRead(FSR_PIN);
    Serial.print("FSR Value: ");
    Serial.println(fsrValue);
    delay(200); // Delay for 1 second before the next reading
}