# BMA456

BMA456 سنسور حرکتی اصلی پروژه و **always-on sentinel** در معماری Low Power است.

| فایل | نوع | وظیفه |
|---|---|---|
| `bma4.c/.h` | Bosch SensorAPI | API پایه BMA4 |
| `bma4_defs.h` | Bosch SensorAPI | registerها و definitions |
| `bma456h.c/.h` | Bosch SensorAPI | feature engine مخصوص BMA456H |
| `bma456_motion.c/.h` | Bolus wrapper | init، XYZ و Step Counter |
| `bma456_event.c/.h` | Bolus wrapper | Any-Motion، INT1 و interrupt status |

`bma456_motion` و `bma456_event` دو مسیر مستقل‌اند: اولی برای accel/step و دومی برای wake/event.

BMA456 روشن می‌ماند تا Step Counter و Any-Motion ادامه داشته باشند. در مسیر فعال telemetry، Step و XYZ فقط هنگام snapshot ارسال خوانده می‌شوند و polling دوره‌ای 500ms وجود ندارد.
