#include <LiquidCrystal.h>

// LCD
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 7, d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// LCD RGB
const int red_pin = 6;
const int green_pin = 9;
const int blue_pin = 10;

int colour[3] = {255, 255, 255};

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

// Screens
#define CLOCK_FACE 0
#define OPTIONS_MENU 1
#define COLOUR_MENU 5
#define DISPLAY_OFF 7

///// END VARIABLES

void setup() {
  pinMode(red_pin, OUTPUT);
  pinMode(green_pin, OUTPUT);
  pinMode(blue_pin, OUTPUT);

  pinMode(enc_a, INPUT_PULLUP);
  pinMode(enc_b, INPUT_PULLUP);

  pinMode(13, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(enc_a), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(enc_b), readEncoder, CHANGE);

  lcd.begin(16, 2);
  
  // Read the user's colour from eeprom?
  analogWrite(green_pin, 255);

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
    case COLOUR_MENU:
      colourMenu();
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
  }

  static const char *option_names[8] = {"Back", "Timer", "Stopwatch", "Alarm", "World Clock", "Colours", "Clock Setup", "Display Off"};
  static const char option_symbols[8] = {'B', 'T', 'S', 'A', 'W', 'C', 'N', 'O'};

  int selected = counter % 8;
  if(selected < 0){
    selected = 8 - abs(selected);
  }

  if(inputs[0] == true){
    switch(selected){
      case 0:
        enterScreen(CLOCK_FACE, 0);
        break;
      case 5:
        enterScreen(COLOUR_MENU, 0);
        break;
      case 7:
        enterScreen(DISPLAY_OFF, 0);
        break;
    }

    lcd.noCursor(); // Disable cursor so it doesn't appear outside of here
    return;
  }


  if(selected != prev_selected){
    lcd.clear();
    lcd.noCursor();

    lcd.setCursor(4, 1);
    for(int i = 0; i < 8; i++){
      lcd.print(option_symbols[i]);
    }

    int leftmost_char = 16 - (strlen(option_names[selected]));
    lcd.setCursor(leftmost_char, 0);
    lcd.print(option_names[selected]);

    lcd.cursor();
    lcd.setCursor(selected + 4, 1); // To show the cursor at the selected option.

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
    lcd.print("Press when done >");
  }

  if(c_menu_stage == 0){
    final_hue = abs(counter % 360);
    updateColour(final_hue, 50);
  }else if(c_menu_stage == 1){
    if(counter > 100){
      counter = 100;
    }else if(counter < 0){
      counter = 0;
    }

    final_lightness = counter;
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
      lcd.print("Choose Shade");
    }else if(c_menu_stage == 1){
      // c_menu_stage = 2; For changing contrast
      enterScreen(OPTIONS_MENU, 0);
      return;
    }
  }

}

void clockFace(){

  static bool show_colon = false;
  static unsigned long colon_last_millis = 0;

  if(inputs[0] == true){
    enterScreen(OPTIONS_MENU, 0);
    return;
  }
  
  char time_str[6];
  if(show_colon){
    strcpy(time_str, "09:41"); // Look into snprintf to assign the hour and minute values as required
  }else{
    strcpy(time_str, "09 41");
  }

  if(millis() - colon_last_millis > 1000){
    show_colon = !show_colon;
    colon_last_millis = millis();
  }

  lcd.setCursor(1, 0);
  lcd.print(time_str);

  char date_str[6];
  strcpy(date_str, "06/12");

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


void updateColour(float hue, int u_lightness) {
  // Implement changing Lightness to allow for different shades. Hue = pick colour, Lightness = pick shade, Sat will always be 1

  hue = hue / 360;

  double r, g, b;
  static float lightness = u_lightness / 100.0;
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
}

double hue2rgb(double p, double q, double t){
	if (t < 0) t += 1;
	if (t > 1) t -= 1;
	if (t < 1 / 6.0) return p + (q - p) * 6 * t;
	if (t < 1 / 2.0) return q;
	if (t < 2 / 3.0) return p + (q - p) * (2 / 3.0 - t) * 6;
	return p;
}
