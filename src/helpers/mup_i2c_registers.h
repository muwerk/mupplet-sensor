// mup_i2c_registers.h
#pragma once

#ifndef tiny_twi_h
#include <Wire.h>
#else
typedef TinyWire TwoWire;
#endif

class I2CRegisters {
  public:
    enum I2CError { UNDEFINED,
                    OK,
                    I2C_HW_ERROR,
                    I2C_WRONG_HARDWARE_AT_ADDRESS,
                    I2C_DEVICE_NOT_AT_ADDRESS,
                    I2C_REGISTER_WRITE_ERROR,
                    I2C_VALUE_WRITE_ERROR,
                    I2C_WRITE_DATA_TOO_LONG,
                    I2C_WRITE_NACK_ON_ADDRESS,
                    I2C_WRITE_NACK_ON_DATA,
                    I2C_WRITE_ERR_OTHER,
                    I2C_WRITE_TIMEOUT,
                    I2C_WRITE_INVALID_CODE,
                    I2C_READ_REQUEST_FAILED };
    I2CError lastError;
    TwoWire *pWire;
    uint8_t i2c_address;
    I2CRegisters(TwoWire *pWire, uint8_t i2c_address)
        : pWire(pWire), i2c_address(i2c_address) {
        // NOTE: Do NOT perform checkAdress() here, since that causes some sensors to simply malfunction. (Example: GDK101)
        lastError = I2CRegisters::I2CError::UNDEFINED;
    }
    ~I2CRegisters() {
    }

    I2CError checkAddress(uint8_t address) {
        pWire->beginTransmission(address);
        byte error = pWire->endTransmission();
        if (error == 0) {
            lastError = I2CError::OK;
            return lastError;
        } else if (error == 4) {
            lastError = I2CError::I2C_HW_ERROR;
        }
        lastError = I2CError::I2C_DEVICE_NOT_AT_ADDRESS;
        return lastError;
    }

    bool endTransmission(bool releaseBus) {
        uint8_t retCode = pWire->endTransmission(releaseBus);  // true: release bus, send stop
        switch (retCode) {
        case 0:
            lastError = I2CError::OK;
            return true;
        case 1:
            lastError = I2CError::I2C_WRITE_DATA_TOO_LONG;
            return false;
        case 2:
            lastError = I2CError::I2C_WRITE_NACK_ON_ADDRESS;
            return false;
        case 3:
            lastError = I2CError::I2C_WRITE_NACK_ON_DATA;
            return false;
        case 4:
            lastError = I2CError::I2C_WRITE_ERR_OTHER;
            return false;
        case 5:
            lastError = I2CError::I2C_WRITE_TIMEOUT;
            return false;
        default:
            lastError = I2CError::I2C_WRITE_INVALID_CODE;
            return false;
        }
        return false;
    }

    bool readRegisterByte(uint8_t reg, uint8_t *pData, bool stop = true, bool allow_irqs = true) {
        *pData = (uint8_t)-1;
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg, 1) != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (endTransmission(stop) == false) {
            if (!allow_irqs) interrupts();
            return false;
        }
        uint8_t read_cnt = pWire->requestFrom(i2c_address, (uint8_t)1, (uint8_t) true);
        if (read_cnt != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2C_READ_REQUEST_FAILED;
            return false;
        }
        *pData = pWire->read();
        if (!allow_irqs) interrupts();
        return true;
    }

    bool readRegisterWord(uint8_t reg, uint16_t *pData, bool stop = true, bool allow_irqs = true) {
        *pData = (uint16_t)-1;
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg, 1) != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (endTransmission(stop) == false) {
            if (!allow_irqs) interrupts();
            return false;
        }
        uint8_t read_cnt = pWire->requestFrom(i2c_address, (uint8_t)2, (uint8_t) true);
        if (read_cnt != 2) {
            if (!allow_irqs) interrupts();
            lastError = I2C_READ_REQUEST_FAILED;
            return false;
        }
        uint8_t hb = pWire->read();
        uint8_t lb = pWire->read();
        if (!allow_irqs) interrupts();
        uint16_t data = (hb << 8) | lb;
        *pData = data;
        return true;
    }

    bool readRegisterWordLE(uint8_t reg, uint16_t *pData, bool stop = true, bool allow_irqs = true) {
        *pData = (uint16_t)-1;
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg, 1) != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (endTransmission(stop) == false) {
            if (!allow_irqs) interrupts();
            return false;
        }
        uint8_t read_cnt = pWire->requestFrom(i2c_address, (uint8_t)2, (uint8_t) true);
        if (read_cnt != 2) {
            lastError = I2C_READ_REQUEST_FAILED;
            if (!allow_irqs) interrupts();
            return false;
        }
        uint8_t lb = pWire->read();
        uint8_t hb = pWire->read();
        if (!allow_irqs) interrupts();
        uint16_t data = (hb << 8) | lb;
        *pData = data;
        return true;
    }

    bool readRegisterTripple(uint8_t reg, uint32_t *pData, bool stop = true, bool allow_irqs = true) {
        *pData = (uint32_t)-1;
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg, 1) != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (endTransmission(stop) == false) {
            if (!allow_irqs) interrupts();
            return false;
        }
        uint8_t read_cnt = pWire->requestFrom(i2c_address, (uint8_t)3, (uint8_t) true);
        if (read_cnt != 3) {
            if (!allow_irqs) interrupts();
            lastError = I2C_READ_REQUEST_FAILED;
            return false;
        }
        uint8_t hb = pWire->read();
        uint8_t lb = pWire->read();
        uint8_t xlb = pWire->read();
        if (!allow_irqs) interrupts();
        uint32_t data = (hb << 16) | (lb << 8) | xlb;
        *pData = data;
        return true;
    }

    bool writeRegisterByte(uint8_t reg, uint8_t val, bool releaseBus = true, bool allow_irqs = true) {
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg, 1) != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (pWire->write(&val, 1) != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_VALUE_WRITE_ERROR;
            return false;
        }
        auto ret = endTransmission(releaseBus);
        if (!allow_irqs) interrupts();
        return ret;
    }
};