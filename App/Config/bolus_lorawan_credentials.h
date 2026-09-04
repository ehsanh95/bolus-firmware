#ifndef BOLUS_LORAWAN_CREDENTIALS_H
#define BOLUS_LORAWAN_CREDENTIALS_H

/*
 * LoRaWAN ABP credentials for Bolus.
 *
 * The product activation mode is ABP for both laboratory validation and the
 * final firmware path. OTAA credentials are intentionally not part of this
 * configuration.
 *
 * Security rule:
 * - Replace ONLY the placeholder values locally with TTN/ChirpStack values.
 * - Set BOLUS_LORAWAN_CREDENTIALS_PROVISIONED to 1 only after provisioning.
 * - Do not commit real production/session keys to this public repository.
 *
 * Imported stack note:
 * - The repository currently uses I-CUBE-LRWAN LoRaMAC 4.4.7 configured for
 *   LoRaWAN 1.0.3 with USE_LRWAN_1_1_X_CRYPTO == 0.
 * - Therefore the MAC exposes one MIB_NWK_S_KEY. For a LoRaWAN 1.0.x ABP
 *   server session, FNwkSIntKey and SNwkSIntKey must represent the same
 *   network session key. Firmware validates that equality before activation.
 */

#define BOLUS_LORAWAN_ACTIVATION_ABP             1
#define BOLUS_LORAWAN_CREDENTIALS_PROVISIONED    0

/*
 * =========================
 * ABP CREDENTIALS
 * =========================
 *
 * Replace these locally before flashing.
 */
#define BOLUS_LORAWAN_DEV_ADDR                    0x260B1234UL

#define BOLUS_LORAWAN_F_NWK_S_INT_KEY_BYTES \
    { 0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, \
      0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU }

#define BOLUS_LORAWAN_S_NWK_S_INT_KEY_BYTES \
    { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U, \
      0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU, 0x00U }

#define BOLUS_LORAWAN_APP_S_KEY_BYTES \
    { 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU, 0x00U, 0x11U, \
      0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U, 0x99U }

/* LoRaWAN application ports. */
#define BOLUS_LORAWAN_APP_PORT                    2U
#define BOLUS_LORAWAN_DOWNLINK_PORT               3U
#define BOLUS_LORAWAN_CONTROL_UPLINK_PORT         4U

/* Uplink policy. */
#define BOLUS_LORAWAN_ADR_ENABLE                  1
#define BOLUS_LORAWAN_CONFIRMED_UPLINK            0
#define BOLUS_LORAWAN_CONFIRMED_TRIALS            3U
#define BOLUS_LORAWAN_QUEUE_DEPTH                 2U
#define BOLUS_LORAWAN_MAX_APP_PAYLOAD             64U

#endif /* BOLUS_LORAWAN_CREDENTIALS_H */
