#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <DS3231.h>
#include "icons.h"

// LCD
// const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 7, d7 = 8;
// hd44780_pinIO lcd(rs, en, d4, d5, d6, d7);
hd44780_I2Cexp lcd;

// LCD RGB
const int red_pin = 9;
const int green_pin = 10;
const int blue_pin = 11;

int colour[3] = {255, 255, 255};

// RTC
DS3231 rtc;
int year = 2025;
int month = 2;
int day = 2;
int hour = 22;
int minute = 20;
int second = 0;
bool dst = false;


// Encoder
const int enc_a = 3;
const int enc_b = 2;
const int enc_sw = 13;

unsigned long last_inc_read_time = micros();
unsigned long last_dec_read_time = micros();
int pause_length = 25000;

volatile int counter = 0;

// Program
bool inputs[2] = {false, false}; // States of the input buttons at each loop.
bool btn_lock[2] = {false, false}; // Button lock for each of the input buttons.
bool held = false;

bool timer_active = false;
bool world_clock = false;

int screen = 0;
bool just_entered = true; 

bool stopwatch_running = false;
int stopwatch_begin = 0; // Timestamp of the most recent starting of the stopwatch.
int stopwatch_total = 0; // Total time on the stopwatch. Enables resuming from a pause without the pause time being a part of the total.

// Screens
#define CLOCK_FACE 0
#define OPTIONS_MENU 1
#define TIMER_MENU 2
#define STOPWATCH 3
#define ALARM_MENU 4
#define WORLD_CLOCK_MENU 5
#define COLOUR_MENU 6
#define CLOCK_SETUP 7
#define DISPLAY_OFF 8

///// END VARIABLES

void setup() {

  pinMode(red_pin, OUTPUT);
  pinMode(green_pin, OUTPUT);
  pinMode(blue_pin, OUTPUT);

  pinMode(enc_a, INPUT_PULLUP);
  pinMode(enc_b, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(enc_a), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(enc_b), readEncoder, CHANGE);

  pinMode(13, INPUT_PULLUP);

  int status;
  status = lcd.begin(16, 2);
  if(status){ // Non-zero result indicates failure to initialise LCD
    hd44780::fatalError(status); // Blink status code using onboard LED
  }
  
  // Setup user data e.g. their selected colour, modes, RTC time, etc.
  analogWrite(green_pin, 255);

  rtc.setClockMode(false);
  getClockData();

  Serial.begin(9600);

}

void loop() {

  processInput();

  // Each screen's function manages inputs to process within its own context, inputs that change the active screen, and its own rendering
  switch(screen){
    case CLOCK_FACE:
      clockFace();
      break;
    case OPTIONS_MENU:
      optionsMenu();
      break;
    case STOPWATCH:
      stopwatch();
      break;
    case COLOUR_MENU:
      colourMenu();
      break;
    case CLOCK_SETUP:
      clockSetup();
      break;
    case DISPLAY_OFF:
      displayOff();
      break;
  }

}

void enterScreen(int new_screen, bool expire_input){ // expire_input - index of input to expire in inputs[]
  screen = new_screen;
  just_entered = true;

  if(expire_input > -1){ // Expire the input that was used to enter the new screen, so that it does not trigger any actions in the new screen
    inputs[expire_input] = false;
  }

  lcd.clear(); // No leftovers from the previous screen!

}

void processInput(){
  // PROCESS BUTTONS
  // will need to be updated to support long/short pressing. Short press: action happens upon release, after short hold. Long press: action happens during hold, after a specified time has passed
  inputs[0] = false; inputs[1] = false;

  // Reading is currently inverted - LOW is pressed, HIGH is released.
  if(digitalRead(enc_sw) == LOW && btn_lock[0] == false){
    btn_lock[0] = true;
    inputs[0] = true;
  }else if(digitalRead(enc_sw) == HIGH && btn_lock[0] == true){
    btn_lock[0] = false;
    Serial.println("Unlocked");
  }

  // READ ENCODER ROTATION
  static int last_counter = 0;
  if(counter != last_counter){
    last_counter = counter;
  }

}

void optionsMenu(){

  static int prev_selected;

  if(just_entered){
    counter = 0; // Reset counter so that scrollable menu starts on option 0 (back)
    prev_selected = 1; // selected will initially be 0, therefore this will trigger an initial render of the menu
    just_entered = false;

    // Load initial icons (the selected option will have its icon dynamically updated later)
    //for(int i = 0; i < 8; i++){
      //lcd.createChar(i, icon_lookup[i*2]);
    //}

    lcd.createChar(0, icon_lookup[0]);
    lcd.createChar(1, icon_lookup[1]);
  }

  static const char *option_names[8] = {"Back", "Timer", "Stopwatch", "Alarm", "World Clock", "Colours", "Clock Setup", "Display Off"};

  int selected = counter % 8;
  if(selected < 0){
    selected = 8 - abs(selected);
  }

  if(inputs[0] == true){
    switch(selected){
      case 0:
        enterScreen(CLOCK_FACE, 0);
        break;
      case 2:
        enterScreen(STOPWATCH, 0);
        break;
      case 5:
        enterScreen(COLOUR_MENU, 0);
        break;
      case 6:
        enterScreen(CLOCK_SETUP, 0);
        break;
      case 7:
        enterScreen(DISPLAY_OFF, 0);
        break;
    }

    //lcd.noCursor(); // Disable cursor so it doesn't appear outside of here
    return;
  }


  if(selected != prev_selected){

    //lcd.createChar(selected, icon_lookup[(selected * 2) + 1]); // Invert icon of selected option
    //lcd.createChar(prev_selected, icon_lookup[prev_selected * 2]); // Undo inversion of previously selected option

    lcd.clear();
    //lcd.noCursor();

    lcd.setCursor(4, 1);
    for(int i = 0; i < 8; i++){ // Render icon slots 0-7
      //lcd.write(i);
      
      if(i == selected){
        lcd.write(1);
      }else{
        lcd.write(0);
      }
      
    }

    int leftmost_char = 16 - (strlen(option_names[selected]));
    lcd.setCursor(leftmost_char, 0);
    lcd.print(option_names[selected]);

    lcd.setCursor(0, 1);

    //lcd.cursor();
    //lcd.setCursor(selected + 4, 1); // To show the cursor at the selected option.

    prev_selected = selected;
  }
}

void colourMenu(){

  static int c_menu_stage;
  
  static int final_hue = 0;
  static int final_lightness = 50;

  if(just_entered){
    counter = 0;
    c_menu_stage = 0;
    just_entered = false;
    
    lcd.setCursor(1, 0);
    lcd.print("Set Colour");
    lcd.setCursor(1, 1);
    lcd.print("then press");
  }

  if(c_menu_stage == 0){
    final_hue = abs(counter % 360);
    updateColour(final_hue, 50);
  }else if(c_menu_stage == 1){
    constrain(counter, 0, 100);
    //final_lightness = counter; brightness effect doesn't look very good
    updateColour(final_hue, final_lightness);
  } // Add display contrast?

  analogWrite(red_pin, colour[0]);
  analogWrite(green_pin, colour[1]);
  analogWrite(blue_pin, colour[2]);

  if(inputs[0] == true){
    if(c_menu_stage == 0){
      c_menu_stage = 1;
      counter = 50;
      lcd.setCursor(1, 0);
      lcd.print("Set Brightness");
    }else if(c_menu_stage == 1){
      // c_menu_stage = 2; For changing contrast
      enterScreen(OPTIONS_MENU, 0);
      return;
    }
  }

}

void clockSetup(){

  static int s_menu_stage;

  if(just_entered){
    s_menu_stage = 0;

    counter = hour;

    lcd.setCursor(1, 0);
    lcd.print("Set");
    lcd.setCursor(0, 1);
    lcd.print("Time");

    just_entered = false;
  }

  switch(s_menu_stage){
    case 0:
      timeSelector(&second, &minute, &hour);
      break;
    case 1:
      dateSelector(&day, &month, &year);
      Serial.print("Updated year: ");
      Serial.println(year);
      break;
    case 2:
      // DST setting menu - turn the dial to toggle between on or off (counter % 2)? This case can't be reached for now
      break;
  }

  // Process inputs
  if(inputs[0] == true){
    switch(s_menu_stage){
      case 0:
        lcd.setCursor(0, 1);
        lcd.print("Date");
        counter = year;
        Serial.print("Setting counter to year: ");
        Serial.println(year);
        break;
      case 1:
        Serial.print("Saving year: ");
        Serial.println(year);
        setClockData(); // Save changes to RTC
        enterScreen(OPTIONS_MENU, 0);
        return;
      case 2:
        // DST choice completed - achieve it by setting the RTC time forward or back by 1h
        enterScreen(OPTIONS_MENU, 0);
        return;

    }

    s_menu_stage += 1;
  }

}

// Both this and date selector take pointers and update the values directly, and hence don't need to return anything
void timeSelector(int* secondP, int* minuteP, int* hourP){

  static int ts_menu_stage;

  switch(ts_menu_stage){
    case 0:
      counter = constrain(counter, 0, 23);
      *hourP = counter;
      break;
    case 1:
      counter = constrain(counter, 0, 59);
      *minuteP = counter;
      break;
    case 2:
      counter = constrain(counter, 0, 59);
      *secondP = counter;
      break;
  }

  // Rendering 
  char time_string[9];
  snprintf(time_string, sizeof(time_string), "%02d:%02d:%02d", *hourP, *minuteP, *secondP); // Change snprintf to string functions (strcpy, strcat) if running out of code space
  lcd.setCursor(8, 0);
  lcd.print(time_string);

  int indicator_loc = 8 + ((ts_menu_stage % 3) * 2) + (ts_menu_stage % 3);
  lcd.setCursor(6, 1);
  lcd.print("          "); // Clear existing ^^
  lcd.setCursor(indicator_loc, 1);
  lcd.print("^^");

  // Process inputs
  if(inputs[0] == true){
    switch(ts_menu_stage){
      case 0:
        inputs[0] = false;
        counter = *minuteP;
        break;
      case 1:
        inputs[0] = false;
        counter = *secondP;
        break;
      case 2:
        ts_menu_stage = 0;
        return;
    }

    ts_menu_stage += 1;
  }

}

void dateSelector(int* dayP, int* monthP, int* yearP){

  static int ds_menu_stage;

  switch(ds_menu_stage){
    case 0:
      counter = constrain(counter, 2024, 2100);
      *yearP = counter;
      break;
    case 1:
      counter = constrain(counter, 1, 12);
      *monthP = counter;
      break;
    case 2:
      counter = day_constrain(counter, *monthP, *yearP); // Different range depending on which month is selected.
      *dayP = counter;
      break;
  }  

  // Rendering 
  char date_string[11];
  snprintf(date_string, sizeof(date_string), "%02d/%02d/%d", *dayP, *monthP, *yearP);
  lcd.setCursor(6, 0);
  lcd.print(date_string);

  int indicator_loc = 12 - ((ds_menu_stage % 3) * 2) - (ds_menu_stage % 3);
  lcd.setCursor(6, 1);
  lcd.print("          "); // Clear existing ^^
  lcd.setCursor(indicator_loc, 1);
  if(ds_menu_stage == 0){
    lcd.print("^^^^");
  }else if(ds_menu_stage < 3){
    lcd.print("^^");
  }

  // Process inputs
  if(inputs[0] == true){
    switch(ds_menu_stage){
      case 0:
        inputs[0] = false;
        counter = *monthP;
        break;
      case 1:
        inputs[0] = false;
        counter = *dayP;
        break;
      case 2:
        ds_menu_stage = 0;
        return;
    }

    ds_menu_stage += 1;
  }

}

void stopwatch(){

  static int prev_selected = 0;
  static bool update_menu = false;

  if(just_entered){
    counter = 0;
    prev_selected = 1;
    just_entered = false;

    // Load icons
    for(int i=0; i<6; i++){
      lcd.createChar(i, sw_icon_lookup[i]);
    }
    
    // Swap to pause icon if we are re-entering the stopwatch screen after it has been running in the background
    if(stopwatch_running){ // Better way of doing this that isn't just overwriting the work we did in the for loop above?
      lcd.createChar(2, sw_icon_lookup[6]);
      lcd.createChar(3, sw_icon_lookup[7]);
    }

    lcd.createChar(6, icon_lookup[4]); // Check the lookup index later once main menu swapped over to raised icon selection indicator
  }

  int selected = counter % 3;
  if(selected < 0){
    selected = 3 - abs(selected);
  }

    // Process inputs
  if(inputs[0] == true){
    switch(selected){
      case 0: // Back button
        enterScreen(OPTIONS_MENU, 0);
        return;

      case 1: // Start or pause stopwatch
        if(stopwatch_running){
          // Add runtime until now to the total
          stopwatch_running = false;
          lcd.createChar(2, sw_icon_lookup[2]); // Swap to play icon
          lcd.createChar(3, sw_icon_lookup[3]); 
        }else{
          // Set start time to now
          stopwatch_running = true;
          lcd.createChar(2, sw_icon_lookup[6]); // Swap to pause icon
          lcd.createChar(3, sw_icon_lookup[7]);
        }
        break;

      case 2: // Reset stopwatch
        stopwatch_running = false;
        stopwatch_total = 0; // Will this need to be a long? Intend to enable stopwatch to support up to 23:59:59 of total runtime
        lcd.createChar(2, sw_icon_lookup[2]); // Swap to play icon
        lcd.createChar(3, sw_icon_lookup[3]); 
        break;
    }

    update_menu = true;
  }

  // Render stopwatch time string
  lcd.setCursor(4, 0);
  if(stopwatch_running){
    // total + calculated time since start
    lcd.print("12:34:56");
  }else{
    if(stopwatch_total == 0){
      lcd.print("00:00:00");
    }else{
      // stopwatch_total
      lcd.print("01:01:01");
    }
  }

  // Render stopwatch controls menu
  if(selected != prev_selected || update_menu){
    lcd.setCursor(0, 1);
    for(int i=0; i<3; i++){
      if(i == selected){
        lcd.write(i*2 + 1);
      }else{
        lcd.write(i*2);
      }
    }

    lcd.setCursor(11, 1);
    switch(selected){
      case 0:
        lcd.print(" Back");
        break;
      case 1:
        if(stopwatch_running){
          lcd.print("Pause");
        }else{
          lcd.print("Start");
        }
        break;
      case 2:
        lcd.print("Reset");
        break;
    }

    prev_selected = selected;
    update_menu = false;
  }
}

void clockFace(){

  static bool show_colon = false;
  static unsigned long colon_last_millis = 0;

  if(inputs[0] == true){
    enterScreen(OPTIONS_MENU, 0);
    return;
  }

  getClockData();
  Serial.print("Year: ");
  Serial.println(year);
  
  char time_str[6];
  if(show_colon){
    snprintf(time_str, 6, "%02d:%02d", hour, minute); // Look into snprintf to assign the hour and minute values as required
  }else{
    snprintf(time_str, 6, "%02d %02d", hour, minute);
  }

  if(millis() - colon_last_millis > 1000){
    show_colon = !show_colon;
    colon_last_millis = millis();
  }

  lcd.setCursor(1, 0);
  lcd.print(time_str);

  char date_str[6];
  snprintf(date_str, 6, "%02d/%02d", day, month);

  lcd.setCursor(10, 1);
  lcd.print(date_str);

  if(timer_active){
    // Render timer, bottom left
  }

  if(world_clock){
    // Render world clock, top right
  }else{
    // Render seconds progressbar, top right
  }

}

void displayOff(){ // What about interrupts for the button here, so the MCU can sleep while display is off?

  if(just_entered){
    lcd.noDisplay();

    analogWrite(red_pin, 0);
    analogWrite(green_pin, 0);
    analogWrite(blue_pin, 0);

    just_entered = false;
  }

  if(inputs[0] == true){
    enterScreen(CLOCK_FACE, 0);

    lcd.display();

    analogWrite(red_pin, colour[0]);
    analogWrite(green_pin, colour[1]);
    analogWrite(blue_pin, colour[2]);
  }

}


void readEncoder() {

  static uint8_t old_AB = 3;
  static int8_t encval = 0;
  static const int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

  old_AB <<= 2;

  if(digitalRead(enc_a)) old_AB |= 0x02; // Add current state of pin A
  if(digitalRead(enc_b)) old_AB |= 0x01; // Add current state of pin B

  encval += enc_states[(old_AB & 0x0f)];

  if(encval > 3){ // Four steps forward
    int change_value = 1;
    if(micros() - last_inc_read_time < pause_length){
      change_value *= 3;
    }

    last_inc_read_time = micros();
    counter += change_value;
    encval = 0;
  }else if(encval < -3){ // Four steps backward
    int change_value = -1;
    if(micros() - last_dec_read_time < pause_length){
      change_value *= 3;
    }

    last_dec_read_time = micros();
    counter += change_value;
    encval = 0;
  }

}

void getClockData(){
  second = rtc.getSecond();
  minute = rtc.getMinute();

  bool twelveHour, pmHour;
  hour = rtc.getHour(twelveHour, pmHour);

  day = rtc.getDate();

  bool centuryBit;
  month = rtc.getMonth(centuryBit);

  year = rtc.getYear() + 2000;
}

void setClockData(){
  rtc.setSecond(second);
  rtc.setMinute(minute);
  rtc.setHour(hour);
  rtc.setDate(day);
  rtc.setMonth(month);
  rtc.setYear(year - 2000); // RTC stores 00 ... 99, so we must store year as an offset from 2000 e.g. 2025-2000 -> 25
}


void updateColour(float hue, float lightness) {
  // Implement changing Lightness to allow for different shades. Hue = pick colour, Lightness = pick shade, Sat will always be 1

  hue = hue / 360;

  double r, g, b;
  lightness = lightness / 100.0;
  static const float saturation = 1;

	if (saturation == 0){
		r = g = b = lightness; // achromatic
	}else{
    auto q = lightness < 0.5 ? lightness * (1 + saturation) : lightness + saturation - lightness * saturation;
		auto p = 2 * lightness - q;

		r = hue2rgb(p, q, hue + 1 / 3.0);
		g = hue2rgb(p, q, hue);
		b = hue2rgb(p, q, hue - 1 / 3.0);
	}

	uint8_t red = static_cast<uint8_t>(r * 255);
	uint8_t green = static_cast<uint8_t>(g * 255);
	uint8_t blue = static_cast<uint8_t>(b * 255);

  colour[0] = red;
  colour[1] = green;
  colour[2] = blue;

  /*
  Serial.print("Red = ");
  Serial.print(red);
  Serial.print(" Green = ");
  Serial.print(green);
  Serial.print(" Blue = ");
  Serial.println(blue);
  */
}

double hue2rgb(double p, double q, double t){
	if (t < 0) t += 1;
	if (t > 1) t -= 1;
	if (t < 1 / 6.0) return p + (q - p) * 6 * t;
	if (t < 1 / 2.0) return q;
	if (t < 2 / 3.0) return p + (q - p) * (2 / 3.0 - t) * 6;
	return p;
}

int day_constrain(int value, int month, int year){
  if(value < 1) return 1;
  
  int month_max = 31;
  switch(month){
    case 2:
      month_max = 28;

      if(year % 4 == 0){ // Check leap year
        if(year % 100 == 0){
          if(year % 400 == 0){
            month_max = 29;
          }
        }else{
          month_max = 29;
        }
      }
      break;
    
    case 4:
    case 6:
    case 9:
    case 11:
      month_max = 30;
  }

  if(value > month_max) return month_max;
  return value;
}
