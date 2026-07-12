################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../rt-thread/libcpu/arm/cortex-m4/cpuport.c 

S_UPPER_SRCS += \
../rt-thread/libcpu/arm/cortex-m4/context_gcc.S 

OBJS += \
./rt-thread/libcpu/arm/cortex-m4/context_gcc.o \
./rt-thread/libcpu/arm/cortex-m4/cpuport.o 

S_UPPER_DEPS += \
./rt-thread/libcpu/arm/cortex-m4/context_gcc.d 

C_DEPS += \
./rt-thread/libcpu/arm/cortex-m4/cpuport.d 


# Each subdirectory must supply rules for building sources it contributes
rt-thread/libcpu/arm/cortex-m4/%.o: ../rt-thread/libcpu/arm/cortex-m4/%.S
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -ffunction-sections -fdata-sections -Wall  -g -gdwarf-2 -x assembler-with-cpp -I"C:\RT-ThreadStudio\workspace\project" -Xassembler -mimplicit-it=thumb -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
rt-thread/libcpu/arm/cortex-m4/%.o: ../rt-thread/libcpu/arm/cortex-m4/%.c
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -ffunction-sections -fdata-sections -Wall  -g -gdwarf-2 -DSOC_FAMILY_STM32 -DSOC_SERIES_STM32F4 -DUSE_HAL_DRIVER -DSTM32F407xx -I"C:\RT-ThreadStudio\workspace\project\drivers" -I"C:\RT-ThreadStudio\workspace\project\drivers\include" -I"C:\RT-ThreadStudio\workspace\project\drivers\include\config" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\Device\ST\STM32F4xx\Include" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\Include" -I"C:\RT-ThreadStudio\workspace\project\libraries\CMSIS\RTOS\Template" -I"C:\RT-ThreadStudio\workspace\project\libraries\STM32F4xx_HAL_Driver\Inc" -I"C:\RT-ThreadStudio\workspace\project\libraries\STM32F4xx_HAL_Driver\Inc\Legacy" -I"C:\RT-ThreadStudio\workspace\project" -I"C:\RT-ThreadStudio\workspace\project\applications" -I"C:\RT-ThreadStudio\workspace\project" -I"C:\RT-ThreadStudio\workspace\project\packages\at_device-latest\class\esp8266" -I"C:\RT-ThreadStudio\workspace\project\packages\at_device-latest\inc" -I"C:\RT-ThreadStudio\workspace\project\packages\cJSON-v1.7.17" -I"C:\RT-ThreadStudio\workspace\project\packages\dht11-latest" -I"C:\RT-ThreadStudio\workspace\project\packages\onenet-latest\inc" -I"C:\RT-ThreadStudio\workspace\project\packages\pahomqtt-latest\MQTTClient-RT" -I"C:\RT-ThreadStudio\workspace\project\packages\pahomqtt-latest\MQTTPacket\src" -I"C:\RT-ThreadStudio\workspace\project\project\packages\at_device-latest\class\esp8266" -I"C:\RT-ThreadStudio\workspace\project\project\packages\at_device-latest\inc" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\dfs\filesystems\devfs" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\dfs\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\drivers\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\drivers\sensors" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\finsh" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\legacy\dfs" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\legacy" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\compilers\common\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\compilers\newlib" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\io\poll" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\io\stdio" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\libc\posix\ipc" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\at\at_socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\at\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\netdev\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\impl" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\dfs_net" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\socket\sys_socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include\socket" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\components\net\sal\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\include" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\libcpu\arm\common" -I"C:\RT-ThreadStudio\workspace\project\rt-thread\libcpu\arm\cortex-m4" -include"C:\RT-ThreadStudio\workspace\project\rtconfig_preinc.h" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

