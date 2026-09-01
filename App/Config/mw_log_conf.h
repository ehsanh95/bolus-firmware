#ifndef __MW_LOG_CONF_H__
#define __MW_LOG_CONF_H__

/*
 * Middleware tracing is intentionally disabled in the first Bolus LoRaWAN
 * integration. Diagnostics are exposed as debugger-visible service counters
 * instead of pulling the ST advanced trace/UART stack into the build.
 */
#define MW_LOG(...) do { } while (0)

#endif /* __MW_LOG_CONF_H__ */
