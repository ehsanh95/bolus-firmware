#ifndef BOLUS_LORAWAN_CREDENTIALS_H
#define BOLUS_LORAWAN_CREDENTIALS_H

/*
 * LoRaWAN OTAA credentials for Bolus.
 *
 * IMPORTANT:
 * - Do not commit production credentials to this public repository.
 * - Keep BOLUS_LORAWAN_CREDENTIALS_PROVISIONED at 0 until credentials are
 *   supplied through a private/local provisioning path.
 * - The zero values below are intentionally non-functional placeholders.
 *
 * LoRaWAN 1.0.x uses one 128-bit root key for OTAA. The integration writes the
 * same root key to both APP_KEY and NWK_KEY MIB entries because the imported
 * Semtech/ST secure-element API exposes the 1.1-style names as well.
 */
#define BOLUS_LORAWAN_CREDENTIALS_PROVISIONED  0

#define BOLUS_LORAWAN_DEV_EUI_BYTES \
    { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U }

#define BOLUS_LORAWAN_JOIN_EUI_BYTES \
    { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U }

#define BOLUS_LORAWAN_ROOT_KEY_BYTES \
    { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, \
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U }

/* Uplink policy. These are engineering defaults, not remotely configurable yet. */
#define BOLUS_LORAWAN_APP_PORT                 2U
#define BOLUS_LORAWAN_ADR_ENABLE               1
#define BOLUS_LORAWAN_CONFIRMED_UPLINK         0
#define BOLUS_LORAWAN_CONFIRMED_TRIALS         3U
#define BOLUS_LORAWAN_JOIN_DATARATE            0  /* EU868 DR0 / SF12 */
#define BOLUS_LORAWAN_JOIN_RETRY_MS             60000UL
#define BOLUS_LORAWAN_QUEUE_DEPTH               2U
#define BOLUS_LORAWAN_MAX_APP_PAYLOAD           64U

#endif /* BOLUS_LORAWAN_CREDENTIALS_H */
