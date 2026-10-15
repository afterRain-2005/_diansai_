################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../key&oled/key.c \
../key&oled/oled.c 

OBJS += \
./key&oled/key.o \
./key&oled/oled.o 

C_DEPS += \
./key&oled/key.d \
./key&oled/oled.d 


# Each subdirectory must supply rules for building sources it contributes
key&oled/%.o key&oled/%.su key&oled/%.cyclo: ../key&oled/%.c key&oled/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DARM_MATH_CM4 -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/CMSIS/DSP/Include -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-key-26-oled

clean-key-26-oled:
	-$(RM) ./key&oled/key.cyclo ./key&oled/key.d ./key&oled/key.o ./key&oled/key.su ./key&oled/oled.cyclo ./key&oled/oled.d ./key&oled/oled.o ./key&oled/oled.su

.PHONY: clean-key-26-oled

