# STM32G4 HAL + CMSIS (vendored)

Vendored (trimmed) subset of the official **STM32Cube_FW_G4 v1.6.3** package,
from:

```
C:/Users/user1/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.3/Drivers
```

Only what the projects in this repo compile against is included:

| Path                                              | What it is                      |
| ------------------------------------------------- | ------------------------------- |
| `CMSIS/Include`                                   | CMSIS 5 core headers (M0..M33)  |
| `CMSIS/Device/ST/STM32G4xx/Include`               | STM32G4 device headers          |
| `STM32G4xx_HAL_Driver/Inc` (+ `Inc/Legacy`)       | STM32G4 HAL headers             |
| `STM32G4xx_HAL_Driver/Src` (all HAL modules)      | STM32G4 HAL sources             |

Not vendored: the package's BSP, DSP/NN/RTOS middleware, example projects,
user manuals (`.chm`), and docs.

To restore the full official package (e.g. for something not vendored here),
configure with `-DSTM32_G4_HAL_ROOT=<root of the full package>` — see the
root `README.md`. License: ST's standard SLA0044 applies to these files.
