#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <m5stack.h>

extern "C"
{
    void app_main(void)
    {
        m5_device_init();
    }
}