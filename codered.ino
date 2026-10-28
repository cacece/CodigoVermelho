#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <avr/io.h>
#include <avr/interrupt.h>

//ricardo falou me falou que alguma das aulas o exemplo usava 0x27, mas no meu Tinkercad o scanner I2C so foi no 0x20
LiquidCrystal_I2C lcd(0x20, 16, 2);

const int pinoPot1  = A0;
const int pinoPot2  = A1;
const int pinoLDR   = A2;
const int pinoTMP36 = A3;
const int pinoStart = 2; 
const int pinoB1 = 3;
const int pinoB2 = 4;
const int pinoB3 = 5;
const int pinoB4 = 6;
const int pinoInclinacao = 7; 
const int pinoUltrassom = 8; 
const int pinoServo = 9;
const int pinoBuzzer = 10;
const int ledVerde = 11;
const int ledAmarelo = 12;
const int ledVermelho = 13;
const int margem_POT  = 5;  
const int margem_LUZ  = 8;  
const int margem_TEMP = 3;  
const int margem_DIST = 5;

int temposFase[3]  = {10, 15, 20};
//int temposFase[3] = {5, 8, 10};
int passosFase[3]  = {4, 6, 8};
int pinosBotoes[4] = {3, 4, 5, 6};

struct DadosJogo {
  int estado;//0 espera, 1 jogando, 2 ok, 3 mudando de fase, 4 falhou e 5 vitoria
  int fase;
  int passo;
  int alvo;
  int tempoGasto;
  int menorTempo;
};
DadosJogo jogo;

int sequencia[10]; //0 pot1, 1 pot2, 2 ldr, 3 tmp36, 4 ultrassom, 5 inclinacao, 6=B1, 7=B2, 8=B3, 9=B4
volatile bool apertouStart = false;
volatile long tempoAcionamento = 0;
volatile bool passouSegundo = false;
volatile int tempoRestante = 0;
unsigned long tempoAnterior = 0;
unsigned long tempoAnteriorServo = 0;
int grauServo = 0;

void atualizaLeds(int vd, int am, int vm){
  digitalWrite(ledVerde, vd);
  digitalWrite(ledAmarelo, am);
  digitalWrite(ledVermelho, vm);
}

void botaoPressionado(){
  static unsigned long tempoPassado = 0;
  unsigned long tempoAgora = millis();    
  if (tempoAgora - tempoPassado > 250){
    apertouStart = true;
    tempoAcionamento = micros();
    tempoPassado = tempoAgora;
  }
}
ISR(TIMER1_COMPA_vect){
  if (jogo.estado == 1){
    if (tempoRestante > 0){
      tempoRestante--;
      jogo.tempoGasto++;
      passouSegundo = true;
    }
  }
}




//leituras
int lerUltrassom(){
  pinMode(pinoUltrassom, OUTPUT);
  digitalWrite(pinoUltrassom, LOW);
  delayMicroseconds(2);
  digitalWrite(pinoUltrassom, HIGH);
  delayMicroseconds(10); 
  digitalWrite(pinoUltrassom, LOW);
  pinMode(pinoUltrassom, INPUT);
  long duracao = pulseIn(pinoUltrassom, HIGH); // igual nos slide da aula 10
  float distancia = duracao * 0.034 / 2; 
  return (int)distancia;
}

int lerTemperatura(){
  int leitura = analogRead(pinoTMP36);
  float tensao = leitura * (5.0 / 1023.0);
  float temperatura = (tensao - 0.5) * 100.0;
  return (int)temperatura;
}

int lerPotenciometro(int pino){
  int valor = analogRead(pino);
  return map(valor, 0, 1023, 0, 100);
}

int lerLuminosidade(){
  // tive q mudar o resistor pra 1k pq 10k tava indo tudo pra esquerda
  int leitura = analogRead(pinoLDR);
  if (leitura < 6) leitura = 6;
  if (leitura > 679) leitura = 679;
  return map(leitura, 6, 679, 0, 100);
}

void controlaServo(){
  if (millis() - tempoAnteriorServo >= 20){
    tempoAnteriorServo = millis();
    int tempoPulso = map(grauServo, 0, 180, 544, 2400); //nas aulas tava (1000us) = 0 graus e 2ms (2000us) = 180 graus, mas assim tava saindo torto
    digitalWrite(pinoServo, HIGH);
    delayMicroseconds(tempoPulso);
    digitalWrite(pinoServo, LOW);
  }
}

void mostraNoLCD(){
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  lcd.print("F");
  lcd.print(jogo.fase + 1);
  lcd.print(" ");
  lcd.print(jogo.passo + 1);
  lcd.print("de");
  lcd.print(passosFase[jogo.fase]);
  lcd.print(" T:");
  if (tempoRestante < 10) lcd.print("0");
  lcd.print(tempoRestante);
  lcd.setCursor(0, 1);
  lcd.print("                ");  //consegue limpa a linha antes de printar o prox comando
  lcd.setCursor(0, 1);
  int item = sequencia[jogo.passo];
  
  if (item == 0){ lcd.print("POT1 = "); lcd.print(jogo.alvo); lcd.print("%"); }
  else if (item == 1){ lcd.print("POT2 = "); lcd.print(jogo.alvo); lcd.print("%"); }
  else if (item == 2){ lcd.print("LUZ = "); lcd.print(jogo.alvo); lcd.print("%"); }
  else if (item == 3){ lcd.print("TEMP = "); lcd.print(jogo.alvo); lcd.print("C"); }
  else if (item == 4){ lcd.print("DIST = "); lcd.print(jogo.alvo); lcd.print("cm"); }
  else if (item == 5){
    if (jogo.alvo == LOW) lcd.print("INCLINE");
    else lcd.print("NIVELE");
  }
  else if (item == 6) lcd.print("APERTE B1");
  else if (item == 7) lcd.print("APERTE B2");
  else if (item == 8) lcd.print("APERTE B3");
  else if (item == 9) lcd.print("APERTE B4");
}

bool verificaAcerto(){
  int item = sequencia[jogo.passo];
  if (item == 0){
    int v = lerPotenciometro(pinoPot1);
    return (v >= jogo.alvo - margem_POT && v <= jogo.alvo + margem_POT);
  }
  if (item == 1){
    int v = lerPotenciometro(pinoPot2);
    return (v >= jogo.alvo - margem_POT && v <= jogo.alvo + margem_POT);
  }
  if (item == 2){
    int v = lerLuminosidade();
    return (v >= jogo.alvo - margem_LUZ && v <= jogo.alvo + margem_LUZ);
  }
  if (item == 3){
    int v = lerTemperatura();
    return (v >= jogo.alvo - margem_TEMP && v <= jogo.alvo + margem_TEMP);
  }
  if (item == 4){
    int v = lerUltrassom();
    return (v >= jogo.alvo - margem_DIST && v <= jogo.alvo + margem_DIST);
  }
  if (item == 5) return digitalRead(pinoInclinacao) == jogo.alvo;
  if (item == 6) return digitalRead(pinoB1) == LOW;
  if (item == 7) return digitalRead(pinoB2) == LOW;
  if (item == 8) return digitalRead(pinoB3) == LOW;
  if (item == 9) return digitalRead(pinoB4) == LOW;
  return false;
}

void sorteiaSequencia(){
  for (int i = 0; i < 10; i++){
    sequencia[i] = i;
  }
  for (int i = 0; i < 10; i++){
    int outro = random(0, 10);
    int temp = sequencia[i];
    sequencia[i] = sequencia[outro];
    sequencia[outro] = temp;
  }
}

void geraAlvo(int item){
  if (item == 0){
    int atual = lerPotenciometro(pinoPot1);
    do { 
      jogo.alvo = random(10, 95); 
    } while (jogo.alvo >= atual - (margem_POT + 5) && jogo.alvo <= atual + (margem_POT + 5));
  }
  else if (item == 1){
    int atual = lerPotenciometro(pinoPot2);
    do { 
      jogo.alvo = random(10, 95); 
    } while (jogo.alvo >= atual - (margem_POT + 5) && jogo.alvo <= atual + (margem_POT + 5));
  }
  else if (item == 2){
    int atual = lerLuminosidade();
    do { 
      jogo.alvo = random(15, 90); 
    } while (jogo.alvo >= atual - (margem_LUZ + 5) && jogo.alvo <= atual + (margem_LUZ + 5));
  }
  else if (item == 3){
    int atual = lerTemperatura();
    do { 
      jogo.alvo = random(0, 75); 
    } while (jogo.alvo >= atual - (margem_TEMP + 4) && jogo.alvo <= atual + (margem_TEMP + 4));
  }
  else if (item == 4){
    int atual = lerUltrassom();
    do { 
      jogo.alvo = random(20, 180); 
    } while (jogo.alvo >= atual - (margem_DIST + 10) && jogo.alvo <= atual + (margem_DIST + 10));
  }
  else if (item == 5){
    int estadoAtual = digitalRead(pinoInclinacao);
    if (estadoAtual == HIGH) jogo.alvo = LOW;
    else jogo.alvo = HIGH;
  }
  else {
    jogo.alvo = LOW;
  }
}

void preparaFase(int f){
  jogo.fase = f;
  jogo.passo = 0;
  sorteiaSequencia();
  geraAlvo(sequencia[jogo.passo]);
  tempoRestante = temposFase[jogo.fase];
  TCNT1 = 0; //sem isso aqui tinha vezes q a proxima fase comecava com menos tempo
  atualizaLeds(LOW, HIGH, LOW); 
  jogo.estado = 1;
  lcd.clear();
  mostraNoLCD();
}

void iniciaPartida(){
  randomSeed(tempoAcionamento + analogRead(pinoPot1));
  jogo.tempoGasto = 0;
  grauServo = 0; 
  tone(pinoBuzzer, 1200, 150);
  preparaFase(0);
}

void setup(){
  for (int i = 0; i < 4; i++){
    pinMode(pinosBotoes[i], INPUT_PULLUP);
  }
  pinMode(pinoStart, INPUT_PULLUP);
  pinMode(pinoInclinacao, INPUT_PULLUP);
  pinMode(pinoServo, OUTPUT);
  pinMode(pinoBuzzer, OUTPUT);
  pinMode(ledVerde, OUTPUT);
  pinMode(ledAmarelo, OUTPUT);
  pinMode(ledVermelho, OUTPUT);

  jogo.estado = 0;
  jogo.menorTempo = 9999;


  
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("MACGYVER CodeRed");
  lcd.setCursor(0, 1);
  lcd.print("APERTE START ");
  atualizaLeds(LOW, LOW, LOW);

  
  
  
  // nao sabia como configurar o timer direito entao copiei a configuracao da Aula 07 e fui pesquisando sobre
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A  = 15624; 
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS12) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A);
  attachInterrupt(digitalPinToInterrupt(pinoStart), botaoPressionado, FALLING);
}

void loop(){
  controlaServo();
  if (apertouStart){
    apertouStart = false;
    if (jogo.estado == 0 || jogo.estado == 4 || jogo.estado == 5){
      iniciaPartida();
      return;
    }
  }

  if (jogo.estado == 1){ //perdeu
    if (tempoRestante <= 0){
      jogo.estado = 4;
      atualizaLeds(LOW, LOW, HIGH);
      tone(pinoBuzzer, 250, 900);   
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("FALHA NA FASE ");
      lcd.print(jogo.fase + 1);
      lcd.setCursor(0, 1);
      lcd.print("START REINICIA");
      return;
    }
    if (passouSegundo){
      passouSegundo = false;
      tone(pinoBuzzer, 850, 40);
      mostraNoLCD();
    }
    if (verificaAcerto()){
      atualizaLeds(HIGH, LOW, LOW); 
      tone(pinoBuzzer, 1800, 120);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print("ACERTOU");
      tempoAnterior = millis(); 
      jogo.estado = 2;
    }
  }

  //pausa pra mostrar correto 
  
  
  
  
  else if (jogo.estado == 2){
    if (millis() - tempoAnterior >= 350){
      jogo.passo++;
      
      if (jogo.passo < passosFase[jogo.fase]){
        geraAlvo(sequencia[jogo.passo]);
        atualizaLeds(LOW, HIGH, LOW);
        jogo.estado = 1;
        mostraNoLCD();
      } 
      else {
        if (jogo.fase == 0){
          grauServo = 60; // avanca o servo
          tone(pinoBuzzer, 2200, 300);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("F1 FEITA");
          lcd.setCursor(0, 1);
          lcd.print("PREPARANDO F2");
          tempoAnterior = millis();
          jogo.estado = 3;
        } 
        else if (jogo.fase == 1){
          grauServo = 120; // avanca servo
          tone(pinoBuzzer, 2400, 300);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("F2 FEITA");
          lcd.setCursor(0, 1);
          lcd.print("PREPARANDO F3");
          tempoAnterior = millis();
          jogo.estado = 3;
        } 
        else {
          grauServo = 180;
          atualizaLeds(HIGH, LOW, LOW);
          tone(pinoBuzzer, 2600, 600);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("MISSAO COMPLETA");
          lcd.setCursor(0, 1);
          
          if (jogo.tempoGasto < jogo.menorTempo){
            jogo.menorTempo = jogo.tempoGasto;
            lcd.print("Novo record. ");
            lcd.print(jogo.tempoGasto);
            lcd.print("s");
          } else {
            lcd.print("Tempo Total: ");
            lcd.print(jogo.tempoGasto);
            lcd.print("s");
          }
          jogo.estado = 5;
        }
      }
    }
  }
  else if (jogo.estado == 3){//tempo entre as fases
    if (millis() - tempoAnterior >= 1200){
      preparaFase(jogo.fase + 1);
    }
  }
}
