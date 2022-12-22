/*
 * control_loop.cpp
 *
 *  Created on: Aug 14, 2022
 *      Author: ayeiser
 */

#include <setup_loop.h>
#include "tim_delay.h"
#include "control.h"
#include "EMFPositionSensor.h"
#include "CurrentSensor.h"
#include "stdio.h"
#include "main.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"


#define ADC_TO_VOLTAGE (3.3f / 4095.0f)
#define CURRENT_SCALE (ADC_TO_VOLTAGE / (50.0f * 0.02f))
#define VBUS_SCALE (ADC_TO_VOLTAGE * 6.1f)

#define DELAY_CORR 0.0f

#define HOBBYKING_MOTOR
//#define PUMP_MOTOR1

#ifdef PUMP_MOTOR1
#define NUM_POLES 3
const vect_dq Ldq = {250e-6f, 250e-6f};
const float R_phase = 3.8f;
#endif

#ifdef HOBBYKING_MOTOR
#define NUM_POLES 6
const vect_dq Ldq = {19e-6f, 19e-6f};
const float R_phase = 0.170f;
#endif


const float dt = 1/float(PWM_FREQ);

extern TIM_HandleTypeDef htim1, htim6;
extern ADC_HandleTypeDef hadc1, hadc2;
extern UART_HandleTypeDef huart3;

CurrentSensorDualADC_BackEMF current(&hadc1, &hadc2,
		CURRENT_SCALE, VBUS_SCALE, ADC_SAMPLES);

EMFPositionSensor pos(current, dt, NUM_POLES, Ldq, R_phase);
TestPositionSensor fakepos(dt, NUM_POLES);

PWMGenerator_WVU pwm(&htim1);

CurrentDQController idqc(dt, 10.0f, Ldq, 0.3f);
GenericController controller(dt, pos, current, pwm, idqc, DELAY_CORR);

TrigAngle angle;

enum ControlMode {
	TORQUE_MODE,
	VELOCITY_MODE,
	STARTUP_MODE,
	LEARNING_MODE
};

uint16_t i = 0;

float mod_meas;
uint32_t phase_meas;
bool foo1, foo2, foo3;
volatile uint32_t motor_angle;

ControlMode mode = TORQUE_MODE;

vect_dq iphase_dq, iphase_dq_slope;

vect_dq idq_target = {0.0f, 0.0f};

void torqueModeHandler(vect_dq idq_target_setpoint) {
	idq_target.d = idq_target_setpoint.d;
	idq_target.q = idq_target_setpoint.q;

	float vbus = current.getVBUS();
	vect_uvw iphase_uvw = current.getCurrents();

	bool tracking = pos.isTracking();

	vect_dq emf_dq;
	pos.getEMF(&emf_dq);

	char str[256];
	int len = sprintf(str, "%0.2f V\t"
			"%0.2f\t"
			"%0.2f V    \t"
			"%0.2f\t%0.2f\t%0.2f A    \t"
			"%0.2f RPM\t"
			"%d\t"
			"%d\n\r",
			vbus,
			pos.getMag(),
			idqc.getMag(),
			iphase_dq.d, iphase_dq.q, iphase_uvw.w,
			pos.getMechVelocity() * (60.0f)/ (2.0f * PI),
			tracking,
			HAL_GPIO_ReadPin(ENTER_GPIO_Port, ENTER_Pin));

	HAL_UART_Transmit(&huart3, (uint8_t*) str, len, HAL_MAX_DELAY);
	HAL_Delay(100);
}

void velocityModeHandler(float speed_target, float kp, float ki) {

}

vect_dq vdq_target = {0.0f, 0.0f};
void learningModeHandler() {

	const int num_volt_targets = 4;
	const int num_speed_targets = 5;

#ifdef HOBBYKING_MOTOR
	float volt_targets[num_volt_targets] = {.3f, .4f, .5f, .6f};
#endif

#ifdef PUMP_MOTOR1
	float volt_targets[num_volt_targets] = {0.5f, 1.0f, 1.5f, 2.0f};
#endif

	float speed_targets[num_speed_targets] = {0.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f};

	vect_dq idq_buf[num_volt_targets*num_speed_targets];
	vect_dq idq_slope_buf[num_volt_targets*num_speed_targets];
	char str[512];

	for (int i = 0; i < num_volt_targets; i++) {
		vdq_target.d = volt_targets[i];
		vdq_target.q = 0.0f;
		for (int j = 0; j < num_speed_targets; j++) {
			float omega = speed_targets[j];
			fakepos.setElecVelocity(omega);
			int len = sprintf(str, "%0.2fV,\t%0.2f rad/s:\t", vdq_target.d, omega);
			HAL_UART_Transmit(&huart3, (uint8_t*) str, len, HAL_MAX_DELAY);
			HAL_Delay(100);
			int index = i*num_speed_targets + j;

			idq_buf[index].d = 0.0f;
			idq_buf[index].q = 0.0f;

			idq_slope_buf[index].d = 0.0f;
			idq_slope_buf[index].q = 0.0f;

			for (int k = 0; k<1000; k++) {
				idq_buf[index].d += iphase_dq.d;
				idq_buf[index].q += iphase_dq.q;
				idq_slope_buf[index].d += iphase_dq_slope.d;
				idq_slope_buf[index].q += iphase_dq_slope.q;
				HAL_Delay(1);
			}
			idq_buf[index].d *= 0.001f;
			idq_buf[index].q *= 0.001f;
			idq_slope_buf[index].d *= 0.001f;
			idq_slope_buf[index].q *= 0.001f;
			len = sprintf(str, "Id = %0.3f\tIq = %0.3f\tdId/dt = %0.2f\tdIq/dt = %0.2f\n\r",
					idq_buf[index].d, idq_buf[index].q, idq_slope_buf[index].d, idq_slope_buf[index].q);
			HAL_UART_Transmit(&huart3, (uint8_t*) str, len, HAL_MAX_DELAY);
		}
	}
	vdq_target.d = 0.0f;
	vdq_target.q = 0.0f;
	HAL_Delay(5000);
	mode = TORQUE_MODE;
}



void setup() {
	HAL_Delay(2000);
	HAL_GPIO_WritePin(USB_ENABLE_GPIO_Port, USB_ENABLE_Pin, GPIO_PIN_SET);

	start_delay_timer(&htim6);
	HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
	HAL_Delay(1000);
	HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
	HAL_Delay(1000);

	current.setup();
	pos.setup();
	pwm.startTiming();
	pwm.startSwitching();
	pwm.update(0, 10.0f);
}

void loop() {
	// main.c while loop goes here

	if (!HAL_GPIO_ReadPin(ENTER_GPIO_Port, ENTER_Pin)) {
		mode = LEARNING_MODE;
	}
	if (mode != LEARNING_MODE && !HAL_GPIO_ReadPin(UP_GPIO_Port, UP_Pin)) {
		fakepos.setElecVelocity(0.0f);
		mode = STARTUP_MODE;
	}
	vect_dq targ = {0.0f, 0.75f};

	switch (mode) {
	case STARTUP_MODE:
	case TORQUE_MODE:
		torqueModeHandler(targ);
		break;
	case VELOCITY_MODE:
//		velocityModeHandler(target_velocity, kp, ki);
		break;
	case LEARNING_MODE:
		learningModeHandler();
		break;
	}
	char usb_str[256];
	int n = sprintf(usb_str, "Hello World! (from USB)\n\r");
	CDC_Transmit_FS((uint8_t*) usb_str, n);
}

volatile bool enabled = false;


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM1) {
		uint32_t raw_angle;
		if (htim->Instance->CR1 & TIM_CR1_DIR) {  // downcounting
			pos.update();
			fakepos.advanceAngle(1.0f);
			switch (mode) {
			case STARTUP_MODE:
			{
				float fakeomega = fakepos.getElecVelocity();
				if (fakeomega > 1000.0f) {
					fakepos.setElecVelocity(0.0f);
					mode = TORQUE_MODE;
				} else if (fakeomega > 250.0f){
					fakepos.setElecVelocity(fakeomega + 500.0f * dt);
				} else {
					fakepos.setElecVelocity(fakeomega + 200.0f * dt);
				}
			}
			case LEARNING_MODE:
				raw_angle = fakepos.calcPhaseAdjAngle(0.25f + DELAY_CORR);
				break;
			default:
				raw_angle = pos.calcPhaseAdjAngle(0.25f + DELAY_CORR);

			}
		} else { // upcounting

			foo1 = current.update(0xff);

			TrigAngle current_angle;
			switch (mode) {
			case LEARNING_MODE:
			case STARTUP_MODE:
				current_angle = fakepos.calcPhaseAdjAngle(DELAY_CORR);
				break;
			default:
				current_angle = pos.calcPhaseAdjAngle(DELAY_CORR);
			}

			vect_uvw iuvw = current.getCurrents();
			vect_uvw iuvw_slope = current.getCurrentSlopes();
			vect_dq idq = current_angle.uvw_to_dq(iuvw);
			iphase_dq = idq;
			iphase_dq_slope = current_angle.uvw_to_dq(iuvw_slope);

			if (!pos.isTracking() && mode != STARTUP_MODE) {
				idq_target.d = 0.0f;
				idq_target.q = 0.0f;
			}

			if (pos.isTracking() && mode == STARTUP_MODE && fakepos.getElecVelocity() > 750.0f) {
				mode = TORQUE_MODE;
			}

			if (current.getVBUS() < 10.0f) {
				enabled = false;
			}
			if (current.getVBUS() > 15.0f) {
				enabled = true;
			}


			if (!enabled) {
				idqc.reset();
				vect_dq vdq = {0.0f, 0.0f};
				pwm.setTargets(vdq);
			} else {
				idqc.setLimit(current.getVBUS() * 0.7f); // lol overmod
				vect_dq vdq;

				switch (mode) {
				case STARTUP_MODE:
					vdq = idqc.update(idq_target, idq, pos.getElecVelocity());
					break;
				case TORQUE_MODE:
					vdq = idqc.update(idq_target, idq, pos.getElecVelocity());
					break;
				case VELOCITY_MODE:
					vdq.d = vdq.q = 0.0f;
					break;
				case LEARNING_MODE:
					vdq = vdq_target;
					break;
				}

				pwm.setTargets(vdq);
			}
			switch (mode) {
			case LEARNING_MODE:
			case STARTUP_MODE:
				raw_angle = fakepos.calcPhaseAdjAngle(0.75f + DELAY_CORR);
				break;
			default:
				raw_angle = pos.calcPhaseAdjAngle(0.75f + DELAY_CORR);
			}
		}
		pwm.update(raw_angle, current.getVBUS());
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
//	HAL_GPIO_WritePin(TIMING_GPIO_Port, TIMING_Pin, GPIO_PIN_SET); // timing pin
	current.retriggerADC(hadc);
//	HAL_GPIO_WritePin(TIMING_GPIO_Port, TIMING_Pin, GPIO_PIN_RESET); // timing pin
}

