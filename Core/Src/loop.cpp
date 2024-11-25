/*
 * control_loop.cpp
 *
 *  Created on: Aug 14, 2022
 *      Author: ayeiser
 */

#include <loop.h>
#include "tim_delay.h"
#include "control.h"
#include "EMFPositionSensor.h"
#include "CurrentSensor.h"
#include "stdio.h"
#include "main.h"
#include "retarget.h"

#include "AS5047PositionSensor.h"
#include "CANHandler.h"
#include "ZettlexIncoder.h"
#include "structs.h"


#define ADC_TO_VOLTAGE (3.3f / 4095.0f)
#define CURRENT_SCALE -(ADC_TO_VOLTAGE / (50.0f * 0.001f)) // 50x shunt gain, 1 mohm shunt
#define VBUS_SCALE (ADC_TO_VOLTAGE * 21.0f) // 200k and 10k divider

#define DELAY_CORR 0.0f

extern TIM_HandleTypeDef htim1, htim6, htim7;
extern ADC_HandleTypeDef hadc1, hadc2;
extern UART_HandleTypeDef huart1;
extern SPI_HandleTypeDef hspi1, hspi3;
extern FDCAN_HandleTypeDef hfdcan1;
extern IWDG_HandleTypeDef hiwdg;

CANHandler canHandler(&hfdcan1);
ZettlexIncoder incoder(&hspi3);

namespace Motor {

#define NUM_POLES 7
const vect_dq Ldq = {250e-6f, 250e-6f};
const float R_phase = 0.3f;

const float dt = 1/float(PWM_FREQ);

ADCReader phaseU(&hadc1, 0, CURRENT_SCALE, 2048);
ADCReader phaseV(&hadc2, 1, CURRENT_SCALE, 2048);
ADCReader phaseW(&hadc2, 0, CURRENT_SCALE, 2048);
ADCReader vbusMeas(&hadc1, 1, VBUS_SCALE, 0);

CurrentSensor current(phaseU, phaseV, phaseW, vbusMeas);
PWMGenerator_WVU pwm(&htim1);
EMFPositionSensor emfpos(current, pwm, dt, NUM_POLES, Ldq, R_phase);
TestPositionSensor fakepos(dt, NUM_POLES);
AS5047PositionSensor as5047pos(&hspi1, dt, NUM_POLES, OFFSET);

LowPassFilter rpmfilter(dt, 100.0f);

CurrentDQController idqc(dt, 10.0f, Ldq, 0.2f);
PIController vel_pi(dt, 0.750f, 0.0f, 0.0f);
//GenericController controller(dt, pos, current, pwm, idqc, DELAY_CORR);

DummyController dummy;
MotorParamFinder params(10.0f);
TorqueController torque(idqc);
//PDVelocityController pdc(idqc, 20.0f);
IncoderPositionController pdc(idqc, incoder, 20.0f);

ParentController* active_controller = &pdc;

motor_command_t motor_command = {
		.position = 0,
		.velocity = 0.0f,
};

motor_reply_t motor_reply = {
		.position = 0,
		.velocity = 0.0f,
		.current = 0.0f,
		.vbus = 0.0f
};

}

const float ParentController::undervoltage_threshold = 15.0f;
const float ParentController::enable_voltage_threshold = 20.0f;
const float ParentController::overvoltage_threshold = 60.0f;
const float ParentController::overcurrent_threshold = 30.0f;

CurrentSensor& ParentController::current = Motor::current;
PWMGenerator& ParentController::pwm = Motor::pwm;
PositionSensor& ParentController::pos = Motor::as5047pos;
//PositionSensor& ParentController::pos = Motor::emfpos;
float ParentController::dt = Motor::dt;

using namespace Motor;


void setup() {
	HAL_Delay(1000);
	RetargetInit(&huart1);
	printf("\n-------------------------------------------------------\n");
	printf(ID_STR "\n");

	printf("Starting CAN...\n");
	canHandler.start();

	printf("Starting ADC reader...\n");
	ADCReader::start();

	printf("Starting htim6...\n");
	start_delay_timer(&htim6);

	printf("Setting up position sensor...\n");
	emfpos.setup();

	as5047pos.setup();

	pdc.setGains(VEL_KP, VEL_KD, PDC_MAX_CURRENT);
	pdc.setPositionGains(0.0f);
	pdc.set_hpf(VEL_HPF);
	pdc.setTargets(as5047pos.getPhaseCorrMechAngle(), 0.0f);
	pdc.setTrajectory(0, 0.0f);


	printf("Setting up PWM generation...\n");
	pwm.startTiming();
	pwm.startSwitching();
	pwm.setAngle(0);
	pwm.update(20.0f);

	HAL_TIM_Base_Start_IT(&htim7);


	printf("Done setup!\n");
}

void loop() {
	// main.c while loop goes here
//	if (active_controller == )

//	if (rxDataReady()) {
//		char c = getchar();
//		printf("c = %c\n", c);
//		if (c == 'x') {
//			ParentController* current = active_controller;
//			active_controller = &params;
//			active_controller->exec();
//			active_controller = current;
//		}
//		if (c == 't') {
//			active_controller = &torque;
//			torque.setTorque(0.0f);
//		}
//		if (c == 'd') {
//			active_controller = &dummy;
//		}
//		if (c == 'p') {
//			active_controller = &pdc;
//			pdc.setGains(100.0f, 0.5f, 10.0f);
//			pdc.setTargets(as5047pos.getPhaseCorrMechAngle(), 0.0f);
//		}
//		if (c == '0') {
//			torque.setTorque(0.0f);
//			pdc.setTargets(0, 0.0f);
//		}
//		if (c == '1') {
//			torque.setTorque(5.0f);
//			pdc.setTargets(0x10000000, 0.0f);
//
//		}
//		if (c == '2') {
//			torque.setTorque(10.0f);
//			pdc.setTargets(0x20000000, 0.0f);
//
//		}
//		if (c == '3') {
//			torque.setTorque(15.0f);
//			pdc.setTargets(0x30000000, 0.0f);
//
//		}
//		if (c == '4') {
//			torque.setTorque(20.0f);
//			pdc.setTargets(0x40000000, 0.0f);
//
//		}
//		if (c == '5') {
//			torque.setTorque(-5.0f);
//			pdc.setTargets(0x50000000, 0.0f);
//
//		}
//		if (c == '6') {
//			torque.setTorque(-10.0f);
//			pdc.setTargets(0x60000000, 0.0f);
//
//		}
//		if (c == '7') {
//			torque.setTorque(-15.0f);
//			pdc.setTargets(0x70000000, 0.0f);
//
//		}
//		if (c == '8') {
//			torque.setTorque(-20.0f);
//			pdc.setTargets(0x80000000, 0.0f);
//
//		}
//		if (c == '9') {
//			torque.setTorque(0.0f);
//			pdc.setTargets(0x90000000, 0.0f);
//
//		}
//
//	}


//	printf("%d 0x%08x    ", incoder.isValid(), incoder.getAngle());
	active_controller->exec();
	HAL_IWDG_Refresh(&hiwdg);

//	uint32_t emf_pos = emfpos.getPhaseCorrElecAngle();
//	uint32_t enc_pos = as5047pos.getPhaseCorrElecAngle();
//	uint32_t diffpos = enc_pos - emf_pos;
//	printf("Positions emf 0x%08x enc 0x%08x diff 0x%08x\n", emf_pos, enc_pos, diffpos);

}

volatile uint32_t invalid_count = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM1) {
//		if (htim->Instance->CR1 & TIM_CR1_DIR) {
//			emfpos.update();
//		}
		if (htim->Instance->CR1 & TIM_CR1_DIR) {
			if (HAL_GetTick() - motor_command.last_tick > 100) {
				pdc.setPositionGains(0.0f);
				pdc.setTrajectory(incoder.getAngle(), 0.0f);
				motor_command.last_tick = HAL_GetTick() - 1000;
			} else {
				if (!incoder.isValid()) {
					invalid_count++;
				} else {
					invalid_count = 0;
				}
				if (invalid_count > 100) {
					invalid_count = 1000;
					motor_command.last_tick = HAL_GetTick() - 1000;
				}
				pdc.setPositionGains(VEL_POS_GAIN);
#ifdef ELEVATION
				pdc.setTrajectory(-motor_command.position, -motor_command.velocity);
#endif

#ifdef AZIMUTH
				pdc.setTrajectory(motor_command.position, motor_command.velocity);
#endif
			}

#ifdef ELEVATION
			motor_reply.position = -incoder.getAngle();
			motor_reply.velocity = -as5047pos.getMechVelocity() / GEAR_REDUCTION;
			motor_reply.current = -ParentController::getCurrents().q;
			motor_reply.vbus = ParentController::getVbus();
#endif

#ifdef AZIMUTH
			motor_reply.position = incoder.getAngle();
			motor_reply.velocity = as5047pos.getMechVelocity() / GEAR_REDUCTION;
			motor_reply.current = ParentController::getCurrents().q;
			motor_reply.vbus = ParentController::getVbus();
#endif
		}
		active_controller->update(htim);
	}

	if (htim->Instance == TIM7) {
		incoder.update();
	}
}

//
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs) {
//	if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET) {
//
//	}
	while(canHandler.popRx(FDCAN_RX_FIFO0) == HAL_OK) {
		canHandler.processRxCAN(&motor_command, &motor_reply);
	}
}

void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo1ITs) {
	while(canHandler.popRx(FDCAN_RX_FIFO1) == HAL_OK) {
		canHandler.processRxCAN(&motor_command, &motor_reply);
	}
}




