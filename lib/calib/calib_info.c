/**
 * \file
 * \brief CryptoAuthLib Basic API methods for Info command.
 *
 * Info command returns a variety of static and dynamic information about the
 * device and its state. Also is used to control the GPIO pin and the persistent
 * latch.
 *
 * \note The ATSHA204A refers to this command as DevRev instead of Info,
 *       however, the OpCode and operation is the same.
 *
 * \note List of devices that support this command - ATSHA204A, ATECC108A,
 *       ATECC508A & ATECC608A/B. There are differences in the modes that they
 *       support. Refer to device datasheets for full details.
 *
 * \copyright (c) 2015-2020 Microchip Technology Inc. and its subsidiaries.
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip software
 * and any derivatives exclusively with Microchip products. It is your
 * responsibility to comply with third party license terms applicable to your
 * use of third party software (including open source software) that may
 * accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
 * EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
 * WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
 * PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT,
 * SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE
 * OF ANY KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF
 * MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE
 * FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL
 * LIABILITY ON ALL CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED
 * THE AMOUNT OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR
 * THIS SOFTWARE.
 */

#include "cryptoauthlib.h"

/** \brief Issues an Info command, which return internal device information and
 *          can control GPIO and the persistent latch.
 *
 * \param[in]  device    Device context pointer
 * \param[in]  mode      Selects which mode to be used for info command.
 * \param[in]  param2    Selects the particular fields for the mode.
 * \param[out] out_data  Response from info command (4 bytes). Can be set to
 *                       NULL if not required.
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */
ATCA_STATUS calib_info_base(ATCADevice device, uint8_t mode, uint16_t param2, uint8_t* out_data)
{
    sudoCliPrintDebug("calInfBase", SUDO_CLI_DEBUG_COMMENT("calib_info_base() function call begin"));
    ATCAPacket packet;
    ATCA_STATUS status = ATCA_GEN_FAIL;

    if (device == NULL)
    {
        sudoCliPrintDebug("calInfBase", SUDO_CLI_DEBUG_COMMENT("device is NULL"));
        return ATCA_TRACE(ATCA_BAD_PARAM, "NULL pointer received");
    }

    // build an info command
    packet.param1 = mode;
    packet.param2 = param2;

    do
    {
        if ((status = atInfo(atcab_get_device_type_ext(device), &packet)) != ATCA_SUCCESS)
        {
            sudoCliPrintDebug("calInfBase", SUDO_CLI_DEBUG_COMMENT("atInfo() failed, break loop"));
            ATCA_TRACE(status, "atInfo - failed");
            break;
        }

        if ((status = atca_execute_command(&packet, device)) != ATCA_SUCCESS)
        {
            // For ECC204, Lock status and Key valid modes return their status in first byte.
            // So, need to consider 01 as valid response as it presents lock/keyvalid status.
            if (((INFO_MODE_LOCK_STATUS == mode) || (INFO_MODE_KEY_VALID == mode))
                && (ECC204 == device->mIface.mIfaceCFG->devtype))
            {
                sudoCliPrintDebug("calInfBase", SUDO_CLI_DEBUG_COMMENT("device is ECC204"));
                if (status == ATCA_CHECKMAC_VERIFY_FAILED)
                {
                    status = ATCA_SUCCESS;
                }
            }
            else
            {
                sudoCliPrintDebug("calInfBase", SUDO_CLI_DEBUG_COMMENT("atca_execute_command() failed, break loop"));
                ATCA_TRACE(status, "calib_info_base - execution failed");
                break;
            }
        }

        uint8_t response = packet.data[ATCA_COUNT_IDX];

        if (response && out_data)
        {
            sudoCliPrintDebug("calInfBase", SUDO_CLI_DEBUG_COMMENT("we got something and can save it somewhere"));
            if (((INFO_MODE_LOCK_STATUS == mode) || (INFO_MODE_KEY_VALID == mode))
                && (ECC204 == device->mIface.mIfaceCFG->devtype))
            {
                sudoCliPrintDebug("calInfBase", SUDO_CLI_DEBUG_COMMENT("device is ECC204"));
                memcpy(out_data, &packet.data[ATCA_RSP_DATA_IDX], 1);
            }
            else if (response >= 7)
            {
                sudoCliPrintDebug("calInfBase", SUDO_CLI_DEBUG_COMMENT("process response"));
                for(int i = 1; i<=4; i++)
                {
                    sudoCliPrintInteger((uint8_t)packet.data[i], 16);
                    sudoCliPrintBytes(SUDO_CLI_STRING(" "));
                }
                sudoCliPrintLine(SUDO_CLI_STRING(" "));

                memcpy(out_data, &packet.data[ATCA_RSP_DATA_IDX], 4);

                for(int i = 0; i<4; i++)
                {
                    sudoCliPrintInteger((uint8_t)out_data[i], 16);
                    sudoCliPrintBytes(SUDO_CLI_STRING(" "));
                }
                sudoCliPrintLine(SUDO_CLI_STRING(" "));
            }
            else
            {
                sudoCliPrintDebug("calInfBase", SUDO_CLI_DEBUG_COMMENT("something is no yes"));
                // do nothing
            }

        }
    }
    while (0);

    return status;
}

/** \brief Use the Info command to get the device revision (DevRev).
 *  \param[in]  device    Device context pointer
 *  \param[out] revision  Device revision is returned here (4 bytes).
 *  \return ATCA_SUCCESS on success, otherwise an error code.
 */
ATCA_STATUS calib_info(ATCADevice device, uint8_t* revision)
{
    if (revision == NULL)
    {
        return ATCA_TRACE(ATCA_BAD_PARAM, "NULL pointer received");
    }

    return calib_info_base(device, INFO_MODE_REVISION, 0, revision);
}

#if CALIB_INFO_LATCH_EN
/** \brief Use the Info command to get the persistent latch current state for
 *          an ATECC608 device.
 *
 *  \param[in]  device  Device context pointer
 *  \param[out] state   The state is returned here. Set (true) or Cler (false).
 *
 *  \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS calib_info_get_latch(ATCADevice device, bool* state)
{
    ATCA_STATUS status = ATCA_GEN_FAIL;
    uint8_t out_data[4];

    if (state == NULL)
    {
        return ATCA_TRACE(ATCA_BAD_PARAM, "NULL pointer received");
    }

    if (ATCA_SUCCESS != (status = calib_info_base(device, INFO_MODE_VOL_KEY_PERMIT, 0, out_data)))
    {
        return ATCA_TRACE(status, "calib_info_base - failed");
    }

    *state = (out_data[0] == 1);

    return status;
}

/** \brief Use the Info command to set the persistent latch state for an
 *          ATECC608 device.
 *
 *  \param[in]  device  Device context pointer
 *  \param[out] state   Persistent latch state. Set (true) or clear (false).
 *
 *  \return ATCA_SUCCESS on success, otherwise an error code.
 */
ATCA_STATUS calib_info_set_latch(ATCADevice device, bool state)
{
    uint16_t param2 = INFO_PARAM2_SET_LATCH_STATE;

    param2 |= state ? INFO_PARAM2_LATCH_SET : INFO_PARAM2_LATCH_CLEAR;
    return calib_info_base(device, INFO_MODE_VOL_KEY_PERMIT, param2, NULL);
}
#endif /* CALIB_INFO_LATCH_EN */

/** \brief Use Info command to check ECC Private key stored in key slot is valid or not
 *
 *  \param[in]   device      Device context pointer
 *  \param[in]   key_id      ECC private key slot id
 *                           For ECC204, key_id is 0x00
 *  \param[out]  is_valid    return private key is valid or invalid
 *
 *  \return ATCA_SUCCESS on success, otherwise an error code.
 */
ATCA_STATUS calib_info_privkey_valid(ATCADevice device, uint16_t key_id, uint8_t* is_valid)
{
    return calib_info_base(device, INFO_MODE_KEY_VALID, key_id, is_valid);
}

#ifdef ATCA_ECC204_SUPPORT
/** \brief Use Info command to ECC204 config/data zone lock status
 *
 *  \param[in]   device      Device context pointer
 *  \param[in]   param2      selects the zone and slot
 *  \param[out]  is_locked   return lock status here
 *
 *  \return ATCA_SUCCESS on success, otherwise an error code.
 */
ATCA_STATUS calib_info_lock_status(ATCADevice device, uint16_t param2, uint8_t* is_locked)
{
    return calib_info_base(device, INFO_MODE_LOCK_STATUS, param2, is_locked);
}
#endif   /* ATCA_ECC204_SUPPORT */
