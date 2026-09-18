# TMP117

درایور TMP117 برای اندازه‌گیری دمای دقیق از طریق I2C.

| فایل | وظیفه |
|---|---|
| `tmp117.c` | register access و temperature conversion |
| `tmp117.h` | register definitions، modeها و API |

در معماری Bolus سنسور عمدتاً در Shutdown Mode است و هنگام نیاز One-Shot خوانده می‌شود:

```text
Shutdown → One-Shot → wait DRDY → read → Shutdown
```

Driver عمومی است؛ timeout، validation، fault reporting و schedule در `SensorService` انجام می‌شود.
