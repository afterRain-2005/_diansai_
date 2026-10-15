################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Tool_Box/ADC_Tools.c \
../Tool_Box/Calculate_Tools.c \
../Tool_Box/Control_Tools.c \
../Tool_Box/Flash_Tools.c \
../Tool_Box/Signal_Tools.c \
../Tool_Box/Svpwm_Tools.c \
../Tool_Box/UI_Tools.c 

OBJS += \
./Tool_Box/ADC_Tools.o \
./Tool_Box/Calculate_Tools.o \
./Tool_Box/Control_Tools.o \
./Tool_Box/Flash_Tools.o \
./Tool_Box/Signal_Tools.o \
./Tool_Box/Svpwm_Tools.o \
./Tool_Box/UI_Tools.o 

C_DEPS += \
./Tool_Box/ADC_Tools.d \
./Tool_Box/Calculate_Tools.d \
./Tool_Box/Control_Tools.d \
./Tool_Box/Flash_Tools.d \
./Tool_Box/Signal_Tools.d \
./Tool_Box/Svpwm_Tools.d \
./Tool_Box/UI_Tools.d 


# Each subdirectory must supply rules for building sources it contributes
Tool_Box/%.o Tool_Box/%.su Tool_Box/%.cyclo: ../Tool_Box/%.c Tool_Box/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DARM_MATH_CM4 -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/CMSIS/DSP/Include -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Tool_Box

clean-Tool_Box:
	-$(RM) ./Tool_Box/ADC_Tools.cyclo ./Tool_Box/ADC_Tools.d ./Tool_Box/ADC_Tools.o ./Tool_Box/ADC_Tools.su ./Tool_Box/Calculate_Tools.cyclo ./Tool_Box/Calculate_Tools.d ./Tool_Box/Calculate_Tools.o ./Tool_Box/Calculate_Tools.su ./Tool_Box/Control_Tools.cyclo ./Tool_Box/Control_Tools.d ./Tool_Box/Control_Tools.o ./Tool_Box/Control_Tools.su ./Tool_Box/Flash_Tools.cyclo ./Tool_Box/Flash_Tools.d ./Tool_Box/Flash_Tools.o ./Tool_Box/Flash_Tools.su ./Tool_Box/Signal_Tools.cyclo ./Tool_Box/Signal_Tools.d ./Tool_Box/Signal_Tools.o ./Tool_Box/Signal_Tools.su ./Tool_Box/Svpwm_Tools.cyclo ./Tool_Box/Svpwm_Tools.d ./Tool_Box/Svpwm_Tools.o ./Tool_Box/Svpwm_Tools.su ./Tool_Box/UI_Tools.cyclo ./Tool_Box/UI_Tools.d ./Tool_Box/UI_Tools.o ./Tool_Box/UI_Tools.su

.PHONY: clean-Tool_Box

