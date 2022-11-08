// mup_i2c_registers.h
#pragma once

#ifndef tiny_twi_h
#include <Wire.h>
#else
typedef TinyWire TwoWire;
#endif

namespace ustd {

/*! \brief mup_i2c_registers.h implements the i2c register read/write protocols used by many sensors

    The I2CRegisters class takes a pointer to a TwoWire object and an i2c address and provides
    a set of functions to read and write registers on the i2c bus.

    This module relies only on the Arudino/ESP Wire library and can be reused
    by any Arduino project.
*/

class I2CRegisters {
  public:
    /*! I2CRegister error codes, many are derived from Arduino's TwoWire */
    enum I2CError { UNDEFINED,                      ///< Not yet initialized
                    OK,                             ///< No error
                    I2C_HW_ERROR,                   ///< Hardware error
                    I2C_WRONG_HARDWARE_AT_ADDRESS,  ///< Wrong hardware at address, e.g. no chip-id check failed
                    I2C_DEVICE_NOT_AT_ADDRESS,      ///< Device not at address, no I2C device found at address
                    I2C_REGISTER_WRITE_ERROR,       ///< Register write error
                    I2C_VALUE_WRITE_ERROR,          ///< Value write error
                    I2C_WRITE_DATA_TOO_LONG,        ///< Write data too long
                    I2C_WRITE_NACK_ON_ADDRESS,      ///< Write NACK on address
                    I2C_WRITE_NACK_ON_DATA,         ///< Write NACK on data
                    I2C_WRITE_ERR_OTHER,            ///< Write error other than NACK on data or address
                    I2C_WRITE_TIMEOUT,              ///< Write timeout
                    I2C_WRITE_INVALID_CODE,         ///< Write invalid code
                    I2C_READ_REQUEST_FAILED,        ///< Read request failed
                    I2C_READ_ERR_OTHER              ///< Copilot invited this error.
    };
    I2CError lastError;
    TwoWire *pWire;
    uint8_t i2cAddress;
    I2CRegisters(TwoWire *pWire, uint8_t i2cAddress)
        : pWire(pWire), i2cAddress(i2cAddress) {
        // NOTE: Do NOT perform checkAdress() here, since that causes some sensors to simply malfunction. (Example: GDK101)
        lastError = I2CRegisters::I2CError::UNDEFINED;
    }
    ~I2CRegisters() {
    }

    I2CError checkAddress(uint8_t address) {
        /*! Check if the device at the given address is present on the i2c bus

        Note: this function is not always safe to use, some i2c devices might end
        up in a strange state, and malfunction as a result of this.

        @param address The i2c address of the device to check
        @return I2CError::OK if the device is present, I2CError::I2C_DEVICE_NOT_AT_ADDRESS otherwise
        */
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
        /*! End the current transmission and release the i2c bus if releaseBus is true
        @param releaseBus If true, release the i2c bus
        @return I2CError::OK if the transaction was successful, an I2CError code otherwise.
        */
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

    bool readRegisterByte(uint8_t reg, uint8_t *pData, bool releaseBus = true, bool allow_irqs = true) {
        /*! Read a single byte from a register on the i2c bus
        @param reg The register to read from
        @param pData Pointer to the byte to read into
        @param releaseBus If true, release the i2c bus
        @param allow_irqs If true, allow interrupts during the read (default, should only be set to false if very high IRQ load is expected)
        */
        *pData = (uint8_t)-1;
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2cAddress);
        if (pWire->write(&reg, 1) != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (endTransmission(releaseBus) == false) {
            if (!allow_irqs) interrupts();
            return false;
        }
        uint8_t read_cnt = pWire->requestFrom(i2cAddress, (uint8_t)1, (uint8_t) true);
        if (read_cnt != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2C_READ_REQUEST_FAILED;
            return false;
        }
        *pData = pWire->read();
        if (!allow_irqs) interrupts();
        return true;
    }

    bool readRegisterWord(uint8_t reg, uint16_t *pData, bool releaseBus = true, bool allow_irqs = true) {
        /*! Read a single word from a register on the i2c bus
        This function reads two bytes from the i2c bus and combines them into a word,
        first byte is most significant byte, second byte is least significant byte. See \ref readRegisterWordLE() for reverse order.
        @param reg The register to read from
        @param pData Pointer to the word to read into
        @param releaseBus If true, release the i2c bus
        @param allow_irqs If true, allow interrupts during the read (default, should only be set to false if very high IRQ load is expected)
        */
        *pData = (uint16_t)-1;
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2cAddress);
        if (pWire->write(&reg, 1) != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (endTransmission(releaseBus) == false) {
            if (!allow_irqs) interrupts();
            return false;
        }
        uint8_t read_cnt = pWire->requestFrom(i2cAddress, (uint8_t)2, (uint8_t) true);
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

    bool readRegisterWordLE(uint8_t reg, uint16_t *pData, bool releaseBus = true, bool allow_irqs = true) {
        /*! Read a single word from a register on the i2c bus
        This function reads two bytes from the i2c bus and combines them into a word,
        first byte is least significant byte, second byte is most significant byte. See \ref readRegisterWord() for reverse order.
        @param reg The register to read from
        @param pData Pointer to the word to read into
        @param releaseBus If true, release the i2c bus
        @param allow_irqs If true, allow interrupts during the read (default, should only be set to false if very high IRQ load is expected)
        */
        *pData = (uint16_t)-1;
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2cAddress);
        if (pWire->write(&reg, 1) != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (endTransmission(releaseBus) == false) {
            if (!allow_irqs) interrupts();
            return false;
        }
        uint8_t read_cnt = pWire->requestFrom(i2cAddress, (uint8_t)2, (uint8_t) true);
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

    bool readRegisterTripple(uint8_t reg, uint32_t *pData, bool releaseBus = true, bool allow_irqs = true) {
        /*! Read three bytes (24bit) from a register on the i2c bus
        This function reads three bytes from the i2c bus and combines them into a dword,
        first byte is most significant byte, second byte is middle byte, third byte is least significant byte.
        @param reg The register to read from
        @param pData Pointer to the dword to read into
        @param releaseBus If true, release the i2c bus
        @param allow_irqs If true, allow interrupts during the read (default, should only be set to false if very high IRQ load is expected)
        */
        *pData = (uint32_t)-1;
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2cAddress);
        if (pWire->write(&reg, 1) != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (endTransmission(releaseBus) == false) {
            if (!allow_irqs) interrupts();
            return false;
        }
        uint8_t read_cnt = pWire->requestFrom(i2cAddress, (uint8_t)3, (uint8_t) true);
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

    bool readRegisterNBytes(uint8_t reg, uint8_t *pData, uint8_t len, bool releaseBus = true, bool allow_irqs = true) {
        /*! Read N bytes from a register on the i2c bus
        This function reads N bytes from the i2c bus into a buffer pData that must have at least N bytes allocated.
        @param reg The register to read from
        @param pData Pointer to the dword to read into
        @param len Number of bytes to read, pData must point to a buffer with at least len bytes allocated
        @param releaseBus If true, release the i2c bus
        @param allow_irqs If true, allow interrupts during the read (default, should only be set to false if very high IRQ load is expected)
        */
        for (uint8_t i = 0; i < len; i++) {
            ((uint8_t *)pData)[i] = 0xFF;
        }
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2cAddress);
        if (pWire->write(&reg, 1) != 1) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (endTransmission(releaseBus) == false) {
            if (!allow_irqs) interrupts();
            return false;
        }
        uint8_t read_cnt = pWire->requestFrom(i2cAddress, (uint8_t)len, (uint8_t) true);
        if (read_cnt != len) {
            if (!allow_irqs) interrupts();
            lastError = I2C_READ_REQUEST_FAILED;
            return false;
        }
        for (uint8_t i = 0; i < len; i++) {
            ((uint8_t *)pData)[i] = pWire->read();
        }
        if (!allow_irqs) interrupts();
        return true;
    }

    bool writeRegisterByte(uint8_t reg, uint8_t val, bool releaseBus = true, bool allow_irqs = true) {
        /*! Write a single byte to a register on the i2c bus
        @param reg The register to write to
        @param val The value to write
        @param releaseBus If true, release the i2c bus
        @param allow_irqs If true, allow interrupts during the write (default, should only be set to false if very high IRQ load is expected)
        */
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2cAddress);
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

    bool writeRegisterNBytes(uint8_t reg, const uint8_t *pData, uint8_t len, bool releaseBus = true, bool allow_irqs = true) {
        /*! Write N bytes to a register on the i2c bus
        @param reg The register to write to
        @param pData Pointer to the data to write, length len
        @param len Number of bytes to write
        @param releaseBus If true, release the i2c bus
        @param allow_irqs If true, allow interrupts during the write (default, should only be set to false if very high IRQ load is expected)
        */
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2cAddress);
        if (pWire->write(&reg, len) != len) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (pWire->write(pData, len) != len) {
            if (!allow_irqs) interrupts();
            lastError = I2CError::I2C_VALUE_WRITE_ERROR;
            return false;
        }
        auto ret = endTransmission(releaseBus);
        if (!allow_irqs) interrupts();
        return ret;
    }
};

}  // namespace ustd