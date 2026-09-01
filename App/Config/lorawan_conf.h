#ifndef __LORAWAN_CONF_H__
#define __LORAWAN_CONF_H__

/*
 * Minimal Bolus configuration for the imported I-CUBE-LRWAN LoRaMAC 4.4.7.
 * Scope of this staging step: EU868, Class A, LoRaWAN 1.0.3, software secure
 * element. Class B, FUOTA and KMS are deliberately disabled.
 */

#define REGION_EU868
#define HYBRID_ENABLED                 0
#define KEY_EXTRACTABLE                0
#define CONTEXT_MANAGEMENT_ENABLED     0
#define LORAMAC_CLASSB_ENABLED         0
#define LORAWAN_KMS                    0

#ifndef CRITICAL_SECTION_BEGIN
#define CRITICAL_SECTION_BEGIN()       UTILS_ENTER_CRITICAL_SECTION()
#endif

#ifndef CRITICAL_SECTION_END
#define CRITICAL_SECTION_END()         UTILS_EXIT_CRITICAL_SECTION()
#endif

#endif /* __LORAWAN_CONF_H__ */
