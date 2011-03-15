#include	<stdint.h>
#include	<string.h>

#include	<avr/interrupt.h>

#include	"intercom.h"
#include	"analog.h"
#include	"config.h"
#include	"watchdog.h"
#include	"heater.h"
#include	"temp.h"

static uint8_t motor_pwm;

void io_init(void) {
	// setup I/O pins
	WRITE(DEBUG_LED, 0); SET_OUTPUT(DEBUG_LED);
	WRITE(H1D,0); SET_OUTPUT(H1D);
	WRITE(H1E,0); SET_OUTPUT(H1E);
	WRITE(H2D,0); SET_OUTPUT(H2D);
	WRITE(H2E,0); SET_OUTPUT(H2E);

	SET_INPUT(TRIM_POT);
	SET_INPUT(TEMP_PIN);
	SET_INPUT(E_STEP_PIN);
	SET_INPUT(E_DIR_PIN);

	// use pull up resistors to avoid noise
	WRITE(E_STEP_PIN, 1);
	WRITE(E_DIR_PIN, 1);

	//Enable the RS485 transceiver
	SET_OUTPUT(RX_ENABLE_PIN);
	SET_OUTPUT(TX_ENABLE_PIN);
	disable_transmit();

	#ifdef	HEATER_PIN
		WRITE(HEATER_PIN, 0); SET_OUTPUT(HEATER_PIN);
	#endif

	#ifdef BED_PIN
		WRITE(BED_PIN, 0); SET_OUTPUT(BED_PIN);
	#endif

// 	#if defined(HEATER_PWM) || defined(FAN_PWM) || defined(BED_PWM)
		// setup PWM timer: fast PWM, no prescaler
		TCCR2A = MASK(WGM21) | MASK(WGM20);
		TCCR2B = MASK(CS22);
		TIMSK2 = 0;
		OCR2A = 0;
		OCR2B = 0;
// 	#endif

	#if defined(H1E_PWM) && defined(H2E_PWM)
		TCCR0A = MASK(WGM01) | MASK(WGM00);
		TCCR0B = MASK(CS20);
		TIMSK0 = 0;
		OCR0A = 0;
		OCR0B = 0;
	#endif
}

void motor_init(void) {
	//Enable an interrupt to be triggered when the step pin changes
	//This will be PCIE0
	PCICR = MASK(PCIE0);
	PCMSK0 = MASK(PCINT2);
}

ISR(PCINT0_vect) {
	static uint8_t coil_pos, pwm;

	//if the step pin is high, we advance the motor
	if (READ(E_STEP_PIN)) {

		//Turn on motors only on first tick to save power I guess
		enable_motors();

		//Advance the coil position
		if (READ(E_DIR_PIN)) 
			coil_pos++;
		else
			coil_pos--;

		coil_pos &= 7;

		//Grab the latest motor power to use
		pwm = motor_pwm;

		switch(coil_pos) {
			case 0:
			  WRITE(H1D, 0);
			  WRITE(H2D, 0);
			  H1E_PWM = 0;
			  H2E_PWM = pwm;
			  break;
			case 1:
			  WRITE(H1D, 1);
			  WRITE(H2D, 0);
			  H1E_PWM = pwm;
			  H2E_PWM = pwm;
			  break;
			case 2:
			  WRITE(H1D, 1);
			  WRITE(H2D, 0);
			  H1E_PWM = pwm;
			  H2E_PWM = 0;
			  break;
			case 3:
			  WRITE(H1D, 1);
			  WRITE(H2D, 1);
			  H1E_PWM = pwm;
			  H2E_PWM = pwm;
			  break;
			case 4:
			  WRITE(H1D, 1);
			  WRITE(H2D, 1);
			  H1E_PWM = 0;
			  H2E_PWM = pwm;  
			  break;
			case 5:
			  WRITE(H1D, 0);
			  WRITE(H2D, 1);
			  H1E_PWM = pwm;
			  H2E_PWM = pwm;  
			  break;
			case 6:
			  WRITE(H1D, 0);
			  WRITE(H2D, 1);
			  H1E_PWM = pwm;
			  H2E_PWM = 0;  
			  break;
			case 7:
			  WRITE(H1D, 0);
			  WRITE(H2D, 0);
			  H1E_PWM = pwm;
			  H2E_PWM = pwm;  
			  break;
		}
	}
}

void init(void) {
	// set up watchdog
	wd_init();

	// setup analog reading
	analog_init();

	// set up serial
	intercom_init();

	// set up inputs and outputs
	io_init();

	// temp sensor
	temp_init();

	// heater
	heater_init();

	// set up extruder motor driver
	motor_init();

	// enable interrupts
	sei();

	// reset watchdog
	wd_reset();
}


int main (void)
{
	init();

	enable_heater();

	start_send();

	// main loop
	for (;;)
	{
		wd_reset();

		//Read motor PWM
		motor_pwm = analog_read(TRIM_POT_CHANNEL) >> 2;

		temp_sensor_tick();

		update_send_cmd(temp_get(0) >> 2);
		temp_set(0, get_read_cmd());
	}
}
