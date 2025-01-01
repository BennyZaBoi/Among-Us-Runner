#include "timerISR.h"
#include "LCD.h"
#include "helper.h"
#include "periph.h"
#include "irAVR.h"
#include <avr/eeprom.h>
#include "serialATmega.h"
#include <stdlib.h>
#include <stdio.h>


#define NUM_TASKS 5



// Among us Character
uint8_t amongus[8] = {
  0B11111,
  0B10001,
  0B10101,
  0B10001,
  0B11111,
  0B11111,
  0B11111,
  0B11011
};

uint8_t bottomspikes[8] = {
    0B00000,
  0B00000,
  0B00000,
  0B00100,
  0B01110,
  0B11111,
  0B11111,
  0B11111
};

//global variables
static bool isJumping = false;
bool musicPlaying = false;
static bool gameStarted = false;
uint8_t score = 0;
static int amongusPos = 1; 

typedef struct _task {
  signed char state;
  unsigned long period;
  unsigned long elapsedTime;
  int (*TickFct)(int);
} task;

const unsigned long GCD = 1;


task tasks[NUM_TASKS];


void TimerISR() {
  unsigned char i;
  for (i = 0; i < NUM_TASKS; ++i) {
    if (tasks[i].elapsedTime >= tasks[i].period) {
      tasks[i].state = tasks[i].TickFct(tasks[i].state);
      tasks[i].elapsedTime = 0;
    }
    tasks[i].elapsedTime += GCD;
  }
}

enum AMONGUS_STATES {AMONGUS_IDLE, AMONGUS_JUMPING};
enum IR_STATES {IR_IDLE};
enum BUZZER_STATES {BUZZER_IDLE, BUZZER_PLAYING};
enum SCORE_STATES { SCORE_INIT, SCORE_UPDATE, SCORE_DISPLAY };
enum GAME_STATES { GAME_INIT, GAME_PLAYING, GAME_OVER };

int TckFct_AMONGUS(int state) {
  uint8_t youPos = 0;
  static int jump = 0;
  switch(state) {
    case AMONGUS_IDLE:
       lcd_goto_xy(1, youPos);
      lcd_write_character(5);
      lcd_goto_xy(0, youPos);
      lcd_write_character(' ');
      if (isJumping) {
        state = AMONGUS_JUMPING;
      }
      break;
    case AMONGUS_JUMPING:
      lcd_goto_xy(0, youPos);
      lcd_write_character(5);
      lcd_goto_xy(1, youPos);
      lcd_write_character(' ');
      if (jump < 5) {
        state = AMONGUS_JUMPING;
        jump++;
        amongusPos = 16;
      }
      else {
        state = AMONGUS_IDLE;
        isJumping = false;
        jump = 0;
        amongusPos = 1;

      }
      break;
    default:
      state = AMONGUS_IDLE;
      break;
  }
  return state;

//actions
  switch(state) {
    case AMONGUS_IDLE:
      break;
    case AMONGUS_JUMPING:
      break;
    default:
      state = AMONGUS_IDLE;
      break;
  }
}


int TckFct_IR(int state) {
    decode_results results; //not global
    switch (state) {
        case IR_IDLE:
            if (IRdecode(&results)) {
                if (results.value == 16748655) { // go up
                    isJumping = true;
                }
                else if (results.value == 16712445) { // turn on music
                    musicPlaying = !musicPlaying;
                }
                else if (results.value == 16753245) { // start game
                    gameStarted = true;
                }

                IRresume(); // Prepare for the next signal
            }
            break;
        default:
            state = IR_IDLE;
            break;
    }
    return state;
}

int TckFct_Buzzer(int state) {
    int tones[] = {659, 659, 659, 523, 659, 784, 392, 
    523, 392, 330, 440, 494, 466, 440, 
    392, 659, 784, 880, 698, 784, 659,
    523, 587, 494, 523, 659, 659, 659, 523, 659, 784, 392, 
    523, 392, 330, 440, 494, 466, 440, 
    392, 659, 784, 880, 698, 784, 659,
    523, 587, 494, 523     };
    static int currentNote = 0;
    switch (state) {
        case BUZZER_IDLE:
            if (musicPlaying) {
                state = BUZZER_PLAYING;
                currentNote = 0;
            }
            else {
                state = BUZZER_IDLE;
            }
            break;
        case BUZZER_PLAYING:
            if (!musicPlaying) {
                currentNote = 0;
                state = BUZZER_IDLE;
            }
            else {
                currentNote = ((currentNote + 1) % 64);
                state = BUZZER_PLAYING;
            }
            break;
        default:
            state = BUZZER_IDLE;
            break;
    }

    switch (state) {
        case BUZZER_IDLE:
            OCR1A = 0;
            break;
        case BUZZER_PLAYING:
            OCR1A = tones[currentNote]/2;
            ICR1 = tones[currentNote];
            break;
        default:
            break;
    }
    return state;
}



int TckFct_Score(int state) {
    serial_println(state);
    char scoreStr[10];
     static uint16_t highScore = 0; // High score
    switch (state) { // Transitions
        case SCORE_INIT:
            score = 0;
            highScore = eeprom_read_word((uint16_t*) 0);
            state = SCORE_UPDATE;
            break;
        case SCORE_UPDATE:
            //if (death) { // Update score only if not game over
                //score++;
                if (score > highScore) {
                    highScore = score;
                    eeprom_update_word((uint16_t*) 0, highScore);
                }
            //}
        
            state = SCORE_DISPLAY;
            break;
        case SCORE_DISPLAY:
            state = SCORE_UPDATE;
            break;
        default:
            state = SCORE_INIT;
            break;
    }

    // Actions
    switch (state) { // Actions
        case SCORE_INIT:
            break;
        case SCORE_UPDATE:
            // lcd_goto_xy(0, 15);     // Top right corner
            // lcd_write_character('0');
            //   lcd_goto_xy(1, 15);     // Bottom right corner  
            // lcd_write_character('0');
            break; // Score is incremented in transition

        case SCORE_DISPLAY:
            lcd_goto_xy(0, 14);
            itoa (highScore, scoreStr, 10);
            serial_println(scoreStr);
            lcd_write_str(scoreStr);
            // Update current score (bottom right)
            lcd_goto_xy(1, 14);
            itoa (score, scoreStr, 10);
            lcd_write_str(scoreStr);
            break;
    }
    return state;
}

int TckFctGame(int state) {
    static int spikePosition = 15; //not global
    static int spikeTimer = 0; //not global
    static bool death = false; //not global
    switch (state) {
        case GAME_INIT:
            lcd_goto_xy(0, 0);
            lcd_write_str("Press Power");
            if (gameStarted == true) {
                lcd_clear();
                state = GAME_PLAYING;
            }
            else {
                state = GAME_INIT;
            }
            break;
            
        case GAME_PLAYING:
            if (amongusPos == spikePosition) {
                state = GAME_OVER;
                death = true;
            }
            else {
                state = GAME_PLAYING;
            }
            break;
            
        case GAME_OVER:
            if (gameStarted) {
                state = GAME_INIT;
                score = 0;
                lcd_clear();
                death = false;
            }
            else {
                state = GAME_OVER;
            }
            break;
    }
    switch(state) {
        case GAME_INIT:
            break;
        case GAME_PLAYING:
        if (spikeTimer < 2){
            spikeTimer++;
        }
        else{
            //summons spikes
            lcd_write_character(' ');
            lcd_goto_xy(1, spikePosition);
            lcd_write_character(' ');
            spikePosition--;
            if (spikePosition == 0){
                spikePosition = 15;
                score++;
            }
            //initialize spikes
            lcd_goto_xy(1, spikePosition);
            lcd_write_character(1);
        }
            break;
        case GAME_OVER:
            lcd_clear();
            lcd_goto_xy(0, 0);
            lcd_write_str("Game Over");
            //initialize the spike
            spikePosition = 15;
            break;
        default:
            state = GAME_INIT;
            break;
    }
    return state;
}

int main(void) {
  ADC_init();
  lcd_init();
  IRinit(&DDRC, &PINC, 0); // Initialize IR sensor
  // Create custom characters
  lcd_create_custom_char(5, amongus); 
  lcd_create_custom_char(1, bottomspikes);
  lcd_goto_xy(0,0);
  
     //lcd_create_custom_char(6, bottomspikes);
  lcd_clear();
  serial_init(9600);
  



  //lcd_write_character(0);
  //lcd_write_character(5);
  //lcd_write_character(2);
  //lcd_write_character(3);
  //lcd_write_character(1);
  //lcd_write_character(5);

  //task periods
  const unsigned long periodAMONGUS = 200;        // Dinosaur animation period
  const unsigned long periodIR = 1;          // IR remote control period
  const unsigned long periodBuzzer = 100;      // Buzzer period
  const unsigned long periodScore = 1000;        // Score period
  const unsigned long periodGame = 300;
  
  //buzzer initialization
  TCCR1A |= (1 << WGM11) | (1 << COM1A1);
  TCCR1B |= (1 << WGM12) | (1 << WGM13) | (1 << CS11); 
  DDRB |= (1 << PB1); // Set PB1 as output

  //task initializations
  tasks[0].state = GAME_INIT;
  tasks[0].period = periodGame;
  tasks[0].elapsedTime = 0;
  tasks[0].TickFct = &TckFctGame;

  tasks[1].state = AMONGUS_IDLE;
  tasks[1].period = periodAMONGUS;
  tasks[1].elapsedTime = 0;
  tasks[1].TickFct = &TckFct_AMONGUS;

  tasks[2].state = IR_IDLE;
  tasks[2].period = periodIR;
  tasks[2].elapsedTime = 0;
  tasks[2].TickFct = &TckFct_IR;

  tasks[3].state = BUZZER_IDLE;
  tasks[3].period = periodBuzzer;
  tasks[3].elapsedTime = 0;
  tasks[3].TickFct = &TckFct_Buzzer;


  tasks[4].state = SCORE_INIT;
  tasks[4].period = periodScore;
  tasks[4].elapsedTime = 0;
  tasks[4].TickFct = &TckFct_Score;


  TimerSet(GCD);
  TimerOn();

  while(1){

    // This is to test the IR Remote
//     if (IRdecode(&results)) {
//                 serial_println(results.value);
//                  IRresume(); // Prepare for the next signal
//           }
 }

  return 0;
}