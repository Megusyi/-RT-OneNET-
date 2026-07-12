################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../project/drivers/board.c \
../project/drivers/drv_adc.c \
../project/drivers/drv_can.c \
../project/drivers/drv_clk.c \
../project/drivers/drv_common.c \
../project/drivers/drv_crypto.c \
../project/drivers/drv_dac.c \
../project/drivers/drv_eth.c \
../project/drivers/drv_flash_f4.c \
../project/drivers/drv_gpio.c \
../project/drivers/drv_hwtimer.c \
../project/drivers/drv_lcd.c \
../project/drivers/drv_lcd_mipi.c \
../project/drivers/drv_lptim.c \
../project/drivers/drv_pm.c \
../project/drivers/drv_pulse_encoder.c \
../project/drivers/drv_pwm.c \
../project/drivers/drv_qspi.c \
../project/drivers/drv_rtc.c \
../project/drivers/drv_sdio.c \
../project/drivers/drv_sdram.c \
../project/drivers/drv_soft_i2c.c \
../project/drivers/drv_spi.c \
../project/drivers/drv_usart.c \
../project/drivers/drv_usart_v2.c \
../project/drivers/drv_usbd.c \
../project/drivers/drv_usbh.c \
../project/drivers/drv_wdt.c 

OBJS += \
./project/drivers/board.o \
./project/drivers/drv_adc.o \
./project/drivers/drv_can.o \
./project/drivers/drv_clk.o \
./project/drivers/drv_common.o \
./project/drivers/drv_crypto.o \
./project/drivers/drv_dac.o \
./project/drivers/drv_eth.o \
./project/drivers/drv_flash_f4.o \
./project/drivers/drv_gpio.o \
./project/drivers/drv_hwtimer.o \
./project/drivers/drv_lcd.o \
./project/drivers/drv_lcd_mipi.o \
./project/drivers/drv_lptim.o \
./project/drivers/drv_pm.o \
./project/drivers/drv_pulse_encoder.o \
./project/drivers/drv_pwm.o \
./project/drivers/drv_qspi.o \
./project/drivers/drv_rtc.o \
./project/drivers/drv_sdio.o \
./project/drivers/drv_sdram.o \
./project/drivers/drv_soft_i2c.o \
./project/drivers/drv_spi.o \
./project/drivers/drv_usart.o \
./project/drivers/drv_usart_v2.o \
./project/drivers/drv_usbd.o \
./project/drivers/drv_usbh.o \
./project/drivers/drv_wdt.o 

C_DEPS += \
./project/drivers/board.d \
./project/drivers/drv_adc.d \
./project/drivers/drv_can.d \
./project/drivers/drv_clk.d \
./project/drivers/drv_common.d \
./project/drivers/drv_crypto.d \
./project/drivers/drv_dac.d \
./project/drivers/drv_eth.d \
./project/drivers/drv_flash_f4.d \
./project/drivers/drv_gpio.d \
./project/drivers/drv_hwtimer.d \
./project/drivers/drv_lcd.d \
./project/drivers/drv_lcd_mipi.d \
./project/drivers/drv_lptim.d \
./project/drivers/drv_pm.d \
./project/drivers/drv_pulse_encoder.d \
./project/drivers/drv_pwm.d \
./project/drivers/drv_qspi.d \
./project/drivers/drv_rtc.d \
./project/drivers/drv_sdio.d \
./project/drivers/drv_sdram.d \
./project/drivers/drv_soft_i2c.d \
./project/drivers/drv_spi.d \
./project/drivers/drv_usart.d \
./project/drivers/drv_usart_v2.d \
./project/drivers/drv_usbd.d \
./project/drivers/drv_usbh.d \
./project/drivers/drv_wdt.d 


# Each subdirectory must supply rules for building sources it contributes
project/drivers/%.o: ../project/drivers/%.c
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -ffunction-sections -fdata-sections -Wall  -g -gdwarf-2 -DSOC_FAMILY_STM32 -DSOC_SERIES_STM32F4 -DUSE_HAL_DRIVER -DSTM32F407xx -I"C:\RT-ThreadStudio\workspace\project\drivers" -I"C:\RT-ThreadStudio\workspace\project\drivers\include" -I"C:\RT-ThreadStudio\workspace\project\drivers\include\config" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\Device\ST\STM32F4xx\Include" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\Include" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\RTOS\Template" -I"C:\RT-ThreadStudio\workspace\project\libraries\STM32F4xx_HAL_Driver\Inc" -I"C:\RT-ThreadStudio\workspace\project\libraries\STM32F4xx_HAL_Driver\Inc\Legacy" -I"C:\RT-ThreadStudio\workspace\project" -I"C:\RT-ThreadStudio\workspace\project\applications" -I"C:\RT-ThreadStudio\workspace\project" -I"C:\RT-ThreadStudio\workspace\project\packages\at_device-latest\class\esp8266" -I"C:\RT-ThreadStudio\workspace\project\packages\at_device-latest\inc" -I"C:\RT-ThreadStudio\workspace\project\packages\cJSON-v1.7.17" -I"C:\RT-ThreadStudio\workspace\project\packages\dht11-latest" -I"C:\RT-ThreadStudio\workspace\project\packages\onenet-latest\inc" -I"C:\RT-ThreadStudio\workspace\project\packages\pahomqtt-latest\MQTTClient-RT" -I"C:\RT-ThreadStudio\workspace\project\packages\pahomqtt-latest\MQTTPacket\src" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\dfs\filesystems\devfs" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\dfs\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\drivers\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\drivers\sensors" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\finsh" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\legacy\dfs" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\legacy" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\compilers\common\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\compilers\newlib" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\io\poll" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\io\stdio" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\ipc" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\at\at_socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\at\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\netdev\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\impl" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\dfs_net" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\socket\sys_socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\libcpu\arm\common" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\libcpu\arm\cortex-m4" -include"C:\RT-ThreadStudio\workspace\project\rtconfig_preinc.h" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

