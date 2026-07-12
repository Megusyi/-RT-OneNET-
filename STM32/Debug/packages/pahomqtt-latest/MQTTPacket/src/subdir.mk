################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../packages/pahomqtt-latest/MQTTPacket/src/MQTTConnectClient.c \
../packages/pahomqtt-latest/MQTTPacket/src/MQTTConnectServer.c \
../packages/pahomqtt-latest/MQTTPacket/src/MQTTDeserializePublish.c \
../packages/pahomqtt-latest/MQTTPacket/src/MQTTFormat.c \
../packages/pahomqtt-latest/MQTTPacket/src/MQTTPacket.c \
../packages/pahomqtt-latest/MQTTPacket/src/MQTTSerializePublish.c \
../packages/pahomqtt-latest/MQTTPacket/src/MQTTSubscribeClient.c \
../packages/pahomqtt-latest/MQTTPacket/src/MQTTSubscribeServer.c \
../packages/pahomqtt-latest/MQTTPacket/src/MQTTUnsubscribeClient.c \
../packages/pahomqtt-latest/MQTTPacket/src/MQTTUnsubscribeServer.c 

OBJS += \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTConnectClient.o \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTConnectServer.o \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTDeserializePublish.o \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTFormat.o \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTPacket.o \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTSerializePublish.o \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTSubscribeClient.o \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTSubscribeServer.o \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTUnsubscribeClient.o \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTUnsubscribeServer.o 

C_DEPS += \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTConnectClient.d \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTConnectServer.d \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTDeserializePublish.d \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTFormat.d \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTPacket.d \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTSerializePublish.d \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTSubscribeClient.d \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTSubscribeServer.d \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTUnsubscribeClient.d \
./packages/pahomqtt-latest/MQTTPacket/src/MQTTUnsubscribeServer.d 


# Each subdirectory must supply rules for building sources it contributes
packages/pahomqtt-latest/MQTTPacket/src/%.o: ../packages/pahomqtt-latest/MQTTPacket/src/%.c
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -ffunction-sections -fdata-sections -Wall  -g -gdwarf-2 -DSOC_FAMILY_STM32 -DSOC_SERIES_STM32F4 -DUSE_HAL_DRIVER -DSTM32F407xx -I"C:\RT-ThreadStudio\workspace\project\drivers" -I"C:\RT-ThreadStudio\workspace\project\drivers\include" -I"C:\RT-ThreadStudio\workspace\project\drivers\include\config" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\Device\ST\STM32F4xx\Include" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\Include" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\RTOS\Template" -I"C:\RT-ThreadStudio\workspace\project\libraries\STM32F4xx_HAL_Driver\Inc" -I"C:\RT-ThreadStudio\workspace\project\libraries\STM32F4xx_HAL_Driver\Inc\Legacy" -I"C:\RT-ThreadStudio\workspace\project" -I"C:\RT-ThreadStudio\workspace\project\applications" -I"C:\RT-ThreadStudio\workspace\project" -I"C:\RT-ThreadStudio\workspace\project\packages\at_device-latest\class\esp8266" -I"C:\RT-ThreadStudio\workspace\project\packages\at_device-latest\inc" -I"C:\RT-ThreadStudio\workspace\project\packages\cJSON-v1.7.17" -I"C:\RT-ThreadStudio\workspace\project\packages\dht11-latest" -I"C:\RT-ThreadStudio\workspace\project\packages\onenet-latest\inc" -I"C:\RT-ThreadStudio\workspace\project\packages\pahomqtt-latest\MQTTClient-RT" -I"C:\RT-ThreadStudio\workspace\project\packages\pahomqtt-latest\MQTTPacket\src" -I"C:\RT-ThreadStudio\workspace\project\project\packages\at_device-latest\class\esp8266" -I"C:\RT-ThreadStudio\workspace\project\project\packages\at_device-latest\inc" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\dfs\filesystems\devfs" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\dfs\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\drivers\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\drivers\sensors" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\finsh" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\legacy\dfs" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\legacy" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\compilers\common\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\compilers\newlib" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\io\poll" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\io\stdio" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\ipc" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\at\at_socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\at\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\netdev\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\impl" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\dfs_net" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\socket\sys_socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\libcpu\arm\common" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\libcpu\arm\cortex-m4" -include"C:\RT-ThreadStudio\workspace\project\rtconfig_preinc.h" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

