#include <Arduino.h>

// Serial Communication
const byte numChars = 7;
char receivedChars[numChars]; // lista com 4 chars e o ultimo é o '\0'
boolean newData = false;

// RPM
double RPM_GLOBAL = 1600;




class Switch {
    uint8_t endstop_pin;
    int endstopState;
    
    public:
        Switch(uint8_t pin){
            endstop_pin = pin;
        }

        void enable(){
            pinMode(endstop_pin, INPUT_PULLUP);
        }

        bool isTrigger(){
            endstopState = digitalRead(endstop_pin);
            return (endstopState == LOW);
        }
};


class Stepper {
    uint8_t dir_pin,step_pin,enable_pin;
    double rpm,step_high_delay,step_low_delay,step_frequency;

  public:
        Stepper (double rotpermin, uint8_t steppin, uint8_t dirpin, uint8_t enablepin) {
            dir_pin = dirpin;
            step_pin = steppin;
            enable_pin = enablepin; 

            rpm = rotpermin;
            step_high_delay = 0.1; // [ms], tem que ser > 1.9 microsegundos
            step_low_delay = 60.0*1000/rpm - step_high_delay; // [ms]
            step_frequency = 1/(step_low_delay + step_high_delay); // [Khz]


            if (step_frequency > 210) { // nao ultrapassar os rpm maximos
                Serial.println("ERROR: STEP_FREQUENCY exceeds 210 kHz!");
                while (1);
            }
        }

        void enable() {  // Declarar pins output
            pinMode(enable_pin, OUTPUT);
            pinMode(dir_pin, OUTPUT);
            pinMode(step_pin, OUTPUT);
            
            digitalWrite(enable_pin, LOW);   // Ativar stepper
        }
        
        void step(uint8_t direction){ // Faz um único step
            digitalWrite(dir_pin, direction);   // Sentido ponteiros relógio
            
            digitalWrite(step_pin, HIGH);
            delay(step_high_delay);
            digitalWrite(step_pin, LOW);
            delay(step_low_delay);
        }

        void rotate(double degree,uint8_t direction){ // Faz N-steps para rodar alfa-graus
            const double degree_mod = fmod(degree, 1.8); // Resto da divisão por 1.8
            
            if (degree_mod != 0.0){
                Serial.println("ERROR: ANGULO NAO É MULTIPLO DE 1.8!");
                // while(1);
            } else {
                for(int i = 0; i < degree/1.8; i++){
                    step(direction);
                }
            }
        }


        void turnOff(){
            digitalWrite(enable_pin, HIGH);   // Desativar stepper
        }


        double fmod(double x, double y) {
            return x - y * int(x / y); // Calculate x % y entre doubles
        }
};


class Photodiode {
    uint8_t analog_pin;
    
    public:
        Photodiode(uint8_t pin){
            analog_pin = pin;
        }

        void enable(){
            pinMode(analog_pin, INPUT);
        }

        float getVoltage(){
            analogRead(analog_pin);
            int analogValue = analogRead(analog_pin);
            // float measuredVoltage = analogValue*1.1/1023;
            float measuredVoltage = analogValue;
            return measuredVoltage;
        }
};


class Led {
    uint8_t ledPin;

    public:
        Led(uint8_t pin){
            ledPin = pin;
        }

        void enable(){
            pinMode(ledPin, INPUT);
        }

        void turnOn(){
            digitalWrite(ledPin, HIGH);
        }

        void turnOff(){
            digitalWrite(ledPin, LOW);
        }

};



Switch switch_E0(3);
Switch switch_X(2);
Switch switch_E1(14);
Switch switch_Y(15);
Switch switch_Z(18);

Stepper stepper_E0(RPM_GLOBAL,26,28,24);
Stepper stepper_E1(RPM_GLOBAL,36,34,30);
Stepper stepper_X(RPM_GLOBAL,A0,A1,38);
Stepper stepper_Y(RPM_GLOBAL,A6,A7,A2);
Stepper stepper_Z(RPM_GLOBAL,46,48,A8);

Stepper stepperArray[5] = {stepper_E0,stepper_E1,stepper_X,stepper_Y,stepper_Z};
Switch switchArray[5] = {switch_E0,switch_E1,switch_X,switch_Y,switch_Z};

Photodiode photodiode(A9);

Led led(44);


void setup(){
    analogReference(INTERNAL1V1);
    Serial.begin(9600);
    delay(1000);
    Serial.println("Arduino Ready:");
}


void loop(){
    recvWithEndMarker();
    

    if(newData == true){
        Serial.print("New Data Received:");
        Serial.println(receivedChars);

        // Com switch case não funciona
        if(receivedChars[0] == '0'){
            Serial.println("Inicializar portas e ligar LED");

            switch_E0.enable();
            switch_E1.enable();
            switch_X.enable();
            switch_Y.enable();
            switch_Z.enable();
            
            stepper_E0.enable();
            stepper_E1.enable();
            stepper_X.enable();
            stepper_Y.enable();
            stepper_Z.enable();
            
            photodiode.enable();
            
            led.enable();
            led.turnOn();
        }
        if(receivedChars[0] == '1'){
            Serial.println("Desligar tudo");
            
            stepper_E0.turnOff();
            stepper_E1.turnOff();
            stepper_X.turnOff();
            stepper_Y.turnOff();
            stepper_Z.turnOff();

            led.turnOff();
        }
        if(receivedChars[0] == '2'){
            Serial.println("RESET A TODOS OS GEARS"); // Reset todos os gears a 0 (ao pé do switch)

            uint8_t resetDir = HIGH;

            while(!switch_X.isTrigger()) {
                stepper_X.rotate(1.8,resetDir);
            }
            Serial.println("reset no gear x");

            while(!switch_Y.isTrigger()) {
                stepper_Y.rotate(1.8,resetDir);
            }
            Serial.println("reset no gear y");

            while(!switch_Z.isTrigger()) {
                stepper_Z.rotate(1.8,resetDir);
            }
            Serial.println("reset no gear z");
            
            while(!switch_E0.isTrigger()) {
                stepper_E0.rotate(1.8,resetDir);
            }
            Serial.println("reset no gear e0");

            while(!switch_E1.isTrigger()) {
                stepper_E1.rotate(1.8,resetDir);
            }
            Serial.println("reset no gear e1");

            Serial.println("DEU RESET A TODOS OS GEARS");
        }
        if(receivedChars[0] == '3'){
            // Recebe string do tipo 3abbb
            // a - numero do stepper a rodar 1a5 
            // bbb - angulo 0-3324 a  digitos
            int stepperNumber = receivedChars[1] - '0';
            int angle = (receivedChars[2] - '0') * 100 + (receivedChars[3] - '0') * 10 + (receivedChars[4] - '0');
            

            Serial.print("Rodar stepper numero:");
            Serial.println(stepperNumber);
            Serial.print("Com angulo:");
            Serial.println(angle);
            
            uint8_t dirToReset = HIGH;
            while(!switchArray[stepperNumber-1].isTrigger()) {
                stepperArray[stepperNumber-1].rotate(1.8,dirToReset);
            }

            uint8_t dirToTop = LOW;
            if(stepperNumber > 0 && stepperNumber < 6 && angle >= 0 && angle < 325){
                stepperArray[stepperNumber-1].rotate((double)angle*5.0,dirToTop);
                Serial.println("Gear rodado");
            }else{
                Serial.println("nao vai rodar dados incorretos");
            }
        }
        if(receivedChars[0] == '4'){
            Serial.println("Efetuar varrimento em 5segundos"); // Efetuar varrimento
            delay(5000);
            // Recebe string do tipo 4XXXXX
            // x=0 nao mexe
            // x=1 efetua varrimento
            
            uint8_t dirToLow = LOW;
            double currentAngle = 0;
            for(currentAngle = 0; currentAngle <= 1.8*180*5;currentAngle=currentAngle+1.8){
                Serial.println(photodiode.getVoltage()); // tem que enviar exatamente 1.8*180*5/0.36=900|
                for(int stepperIndex=0;stepperIndex < 5;stepperIndex++){
                    if(receivedChars[stepperIndex+1] == '1'){
                        if(currentAngle == 0){
                            uint8_t dirToReset = HIGH;
                            while(!switchArray[stepperIndex].isTrigger()) {
                                stepperArray[stepperIndex].rotate(1.8,dirToReset);
                            }
                        }else{
                            stepperArray[stepperIndex].step(dirToLow);
                        }
                    }
                }
            }
        }
        if(receivedChars[0] == '5'){
            Serial.println("[0] == 5 RECEBIDO");
        }
        if(receivedChars[0] == '6'){
            Serial.println("[0] == 6 RECEBIDO");
        }
        if(receivedChars[0] == '7'){
            Serial.println("[0] == 7 RECEBIDO");
        }
        if(receivedChars[0] == '8'){
            Serial.println("[0] == 8 RECEBIDO");
        }
        if(receivedChars[0] == '9'){
            Serial.println("Voltagem medida:");
            Serial.println(photodiode.getVoltage());
        }


                // Serial.println("[0] == 8 RECEBIDO");


                // uint8_t dirZ = HIGH;
                // while(!switch_Z.isTrigger()) {
                //     stepper_Z.rotate(1.8,dirZ);
                // } // a 1600RPM demora 20s a dar reset
                // Serial.println("DEU RESET");

                // dirZ = LOW;
                // stepper_Z.rotate(1.8*180*5,dirZ);

                
                // Serial.println("ACABOU CASE 8");


        newData = false;
    }


}


void recvWithEndMarker() {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;
    
    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (rc != endMarker) {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) {
                ndx = numChars - 1;
            }
        }
        else {
            receivedChars[ndx] = '\0'; // terminate the string
            ndx = 0;
            newData = true;
        }
    }
}
