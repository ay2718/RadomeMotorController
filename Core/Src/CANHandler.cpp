/*
 * CANHandler.cpp
 *
 *  Created on: Feb 9, 2023
 *      Author: ayeiser
 */

#include <CANHandler.h>
#include <stdio.h>
#include <cmath>

void CANHandler::start() {

	// Configure the CAN peripheral
	FDCAN_FilterTypeDef sFilterConfig = {0};

	HAL_FDCAN_ConfigGlobalFilter(hfdcan,
			FDCAN_REJECT, FDCAN_REJECT,
			FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);

	sFilterConfig.IdType = FDCAN_STANDARD_ID;
	sFilterConfig.FilterIndex = 0;
	sFilterConfig.FilterType = FDCAN_FILTER_DUAL;
	sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
	sFilterConfig.FilterID1 = ID;
	sFilterConfig.FilterID2 = 0x0;

	if (HAL_FDCAN_ConfigFilter(hfdcan, &sFilterConfig) != HAL_OK)
	{
		Error_Handler();
	}

	sFilterConfig.IdType = FDCAN_EXTENDED_ID;
	sFilterConfig.FilterIndex = 0;
	sFilterConfig.FilterType = FDCAN_FILTER_DUAL;
	sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
	sFilterConfig.FilterID1 = ID;
	sFilterConfig.FilterID2 = 0x0;

	if (HAL_FDCAN_ConfigFilter(hfdcan, &sFilterConfig) != HAL_OK)
	{
		Error_Handler();
	}

	sFilterConfig.IdType = FDCAN_EXTENDED_ID;
	sFilterConfig.FilterIndex = 1;
	sFilterConfig.FilterType = FDCAN_FILTER_DUAL;
	sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO1;
	sFilterConfig.FilterID1 = 0x10000000 + ID;
	sFilterConfig.FilterID2 = 0x10400000 + ID;

	if (HAL_FDCAN_ConfigFilter(hfdcan, &sFilterConfig) != HAL_OK)
	{
		Error_Handler();
	}


	if (HAL_FDCAN_Start(hfdcan) != HAL_OK) {
		Error_Handler();
	}

	if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
		Error_Handler();
	}

	if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO1_NEW_MESSAGE, 0) != HAL_OK) {
		Error_Handler();
	}

	TxHeader.Identifier = 0x69;
	TxHeader.IdType = FDCAN_STANDARD_ID;
	TxHeader.TxFrameType = FDCAN_DATA_FRAME;
	TxHeader.DataLength = FDCAN_DLC_BYTES_8;
	TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
	TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
	TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
	TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
	TxHeader.MessageMarker = 0;
}

bool CANHandler::setupTxPacket(uint32_t id, uint32_t len) {
	TxHeader.Identifier = id;
	if (len <= 8) {
		TxHeader.DataLength = FDCAN_DLC_BYTES_1*len;
		return true;
	} else {
		return false;
	}
}

HAL_StatusTypeDef CANHandler::pushTx() {
	return HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &TxHeader, TxData);
}

HAL_StatusTypeDef CANHandler::pushTx(uint32_t id, uint32_t len) {
	if (!setupTxPacket(id, len)) {
		return HAL_ERROR;
	}
	return pushTx();
}

uint32_t CANHandler::getRxFifoFillLevel(uint32_t fifo) {
	return HAL_FDCAN_GetRxFifoFillLevel(hfdcan, fifo);
}

HAL_StatusTypeDef CANHandler::popRx(uint32_t fifo) {
	return HAL_FDCAN_GetRxMessage(hfdcan, fifo, &RxHeader, RxData);
}

void CANHandler::printRxCAN(void) {
	uint32_t len = RxHeader.DataLength / FDCAN_DLC_BYTES_1;
	uint32_t addr = RxHeader.Identifier;

	if (len > 0 && len <= 8) {
		printf("Rx CAN id: %08x data:", addr);
		for (int i = 0; i < len; i++) {
			printf(" %02x", RxData[i]);
		}
		printf("\n");
	}
}

void CANHandler::processRxCAN(motor_command_t* _cmd, motor_reply_t* _reply) {
	uint32_t len = RxHeader.DataLength / FDCAN_DLC_BYTES_1;
	uint32_t addr = RxHeader.Identifier;

	if (addr == 0x10000000 + ID)
	{
		NVIC_SystemReset();
	}

	if (addr == 0x10400000 + ID)
	{
		// TODO offset learn mode
	}

	if (addr == ID) {
		if (len == 5) {
			_cmd->position = (RxData[0] << 24) | (RxData[1] << 16) | (RxData[2] << 8);
			uint16_t velocity_buf = (RxData[3] << 8) | (RxData[4]);
			_cmd->velocity = float(int16_t(velocity_buf)) * VELOCITY_LSB;
			_cmd->last_tick = HAL_GetTick();

			createReplyPacket(_reply);
		}
	}

	if (addr == 0) {
		createReplyPacket(_reply);
	}
}

void CANHandler::createReplyPacket(motor_reply_t* _reply) {
	setupTxPacket(0x100 + ID, 8);
	TxData[0] = _reply->position >> 24;
	TxData[1] = _reply->position >> 16;
	TxData[2] = _reply->position >> 8;

	uint16_t velo_limited = int16_t(fminf(fmaxf(lroundf(_reply->velocity / VELOCITY_LSB), -32767.0f), 32767.0f));
	TxData[3] = velo_limited >> 8;
	TxData[4] = velo_limited;

	uint16_t current = int16_t(lroundf(_reply->current / CURRENT_LSB));
	TxData[5] = current >> 8;
	TxData[6] = current;

	TxData[7] = lroundf(_reply->vbus / VBUS_LSB);

	pushTx();
}



