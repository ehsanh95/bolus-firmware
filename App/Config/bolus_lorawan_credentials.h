#ifndef BOLUS_LORAWAN_CREDENTIALS_H
#define BOLUS_LORAWAN_CREDENTIALS_H

/*
 * LoRaWAN credentials for Bolus.
 *
 * TEST MODE:
 * - ABP is prepared for laboratory validation of the complete uplink path.
 * - Replace ONLY the placeholder values locally with TTN generated values.
 * - Do not commit real production keys to this repository.
 *
 * Production recommendation:
 * - Use OTAA with secure provisioning and optional NVM session restore.
 */

#define BOLUS_LORAWAN_ACTIVATION_ABP           1
#define BOLUS_LORAWAN_CREDENTIALS_PROVISIONED  0

/*
 * =========================
 * ABP TEST CREDENTIALS
 * =========================
 *
 * TTN provides these values when an ABP device is created:
 *
 * DevAddr:
 *   Example: 260B1234
 *   Stored as 32-bit address.
 *
 * FNwkSIntKey:
 *   Example format from TTN:
 *   00112233445566778899AABBCCDDEEFF
 *
 * SNwkSIntKey:
 *   Example format from TTN:
 *   112233445566778899AABBCCDDEEFF00
 *
 * AppSKey:
 *   Example format from TTN:
 *   AABBCCDDEEFF00112233445566778899
 *
 * Replace these locally before flashing.
 */

#define BOLUS_LORAWAN_DEV_ADDR                  0x260B1234UL

#define BOLUS_LORAWAN_F_NWK_S_INT_KEY_BYTES \
    { 0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, \
      0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU }

#define BOLUS_LORAWAN_S_NWK_S_INT_KEY_BYTES \
    { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U, \
      0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU, 0x00U }

#define BOLUS_LORAWAN_APP_S_KEY_BYTES \
    { 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU, 0x00U, 0x11U, \
      0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U, 0x99U }

/*
 * OTAA placeholders remain available for future production provisioning.
 */
#define BOLUS_LORAWAN_DEV_EUI_BYTES \
    { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U }

#define BOLUS_LORAWAN_JOIN_EUI_BYTES \
    { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U }

#define BOLUS_LORAWAN_ROOT_KEY_BYTES \
    { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, \
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U }

/*
 * LoRaWAN application ports.
 */
#define BOLUS_LORAWAN_APP_PORT                 2U
#define BOLUS_LORAWAN_DOWNLINK_PORT            3U
#define BOLUS_LORAWAN_CONTROL_UPLINK_PORT      4U

/* Uplink policy. */
#define BOLUS_LORAWAN_ADR_ENABLE               1
#define BOLUS_LORAWAN_CONFIRMED_UPLINK         0
#define BOLUS_LORAWAN_CONFIRMED_TRIALS         3U
#define BOLUS_LORAWAN_JOIN_DATARATE            0  /* EU868 DR0 / SF12 */
#define BOLUS_LORAWAN_JOIN_RETRY_MS             60000UL
#define BOLUS_LORAWAN_QUEUE_DEPTH               2U
#define BOLUS_LORAWAN_MAX_APP_PAYLOAD           64U

#endif /* BOLUS_LORAWAN_CREDENTIALS_H */
