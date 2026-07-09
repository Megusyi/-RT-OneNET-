################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../applications/Mq2.c \
../applications/docker.c \
../applications/door_lock.c \
../applications/k230_uart.c \
../applications/main.c \
../applications/onenet_recv.c \
../applications/onenet_sample.c \
../applications/servo.c 

OBJS += \
./applications/Mq2.o \
./applications/docker.o \
./applications/door_lock.o \
./applications/k230_uart.o \
./applications/main.o \
./applications/onenet_recv.o \
./applications/onenet_sample.o \
./applications/servo.o 

C_DEPS += \
./applications/Mq2.d \
./applications/docker.d \
./applications/door_lock.d \
./applications/k230_uart.d \
./applications/main.d \
./applications/onenet_recv.d \
./applications/onenet_sample.d \
./applications/servo.d 


# Each subdirectory must supply rules for building sources it contributes
applications/%.o: ../applications/%.c
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -ffunction-sections -fdata-sections -Wall  -g -gdwarf-2 -DSOC_FAMILY_STM32 -DSOC_SERIES_STM32F4 -DUSE_HAL_DRIVER -DSTM32F407xx -I"C:\RT-ThreadStudio\workspace\project\drivers" -I"C:\RT-ThreadStudio\workspace\project\drivers\include" -I"C:\RT-ThreadStudio\workspace\project\drivers\include\config" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\Device\ST\STM32F4xx\Include" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\Include" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\RTOS\Template" -I"C:\RT-ThreadStudio\workspace\project\libraries\STM32F4xx_HAL_Driver\Inc" -I"C:\RT-ThreadStudio\workspace\project\libraries\STM32F4xx_HAL_Driver\Inc\Legacy" -I"C:\RT-ThreadStudio\workspace\project" -I"C:\RT-ThreadStudio\workspace\project\applications" -I"C:\RT-ThreadStudio\workspace\project" -I"C:\RT-ThreadStudio\workspace\project\packages\at_device-latest\class\esp8266" -I"C:\RT-ThreadStudio\workspace\project\packages\at_device-latest\inc" -I"C:\RT-ThreadStudio\workspace\project\packages\cJSON-v1.7.17" -I"C:\RT-ThreadStudio\workspace\project\packages\dht11-latest" -I"C:\RT-ThreadStudio\workspace\project\packages\onenet-latest\inc" -I"C:\RT-ThreadStudio\workspace\project\packages\pahomqtt-latest\MQTTClient-RT" -I"C:\RT-ThreadStudio\workspace\project\packages\pahomqtt-latest\MQTTPacket\src" -I"C:\RT-ThreadStudio\workspace\project\project\packages\at_device-latest\class\esp8266" -I"C:\RT-ThreadStudio\workspace\project\project\packages\at_device-latest\inc" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\dfs\filesystems\devfs" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\dfs\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\drivers\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\drivers\sensors" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\finsh" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\legacy\dfs" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\legacy" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\compilers\common\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\compilers\newlib" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\io\poll" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\io\stdio" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\ipc" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\at\at_socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\at\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\netdev\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\impl" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\dfs_net" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\socket\sys_socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\libcpu\arm\common" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\libcpu\arm\cortex-m4" -include"C:\RT-ThreadStudio\workspace\project\rtconfig_preinc.h" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

