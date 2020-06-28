/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "ff.h"
#include "usrconf.h"

/*===========================================================================*/
/* SD related.                                                            */
/*===========================================================================*/

/**
 * @brief FS object.
 */
static FATFS SDC_FS;
#define is_fs_ready(fs) fs->fs_type

/* 
* MMC/SD over SPI driver configuration .
*/
static MMCConfig mmccfg = {SPI_DRIVER, &ls_spicfg, &hs_spicfg};
MMCDriver MMCD1;
void mmc_start_with_spi(MMCDriver *mmcd, MMCConfig *cfg);
void spi_pin_config(void);

/* 
* SD/FS related functions.
*/
BYTE sd_mount(FATFS *fs, MMCDriver *mmcd);
static FRESULT scan_files(BaseSequentialStream *chp, char *path);

/*===========================================================================*/
/* Main and generic code.                                                    */
/*===========================================================================*/

/*
* SD initialization.
*/
bool sd_init(MMCDriver *mmcd, MMCConfig *mmcconfig);

/*
* Application entry point.
*/
int main(void) {
    /*
    * System initializations.
    * - HAL initialization, this also initializes the configured device drivers
    *   and performs the board-specific initializations.
    * - Kernel initialization, the main() function becomes a thread and the
    *   RTOS is active.
    */
    halInit();
    chSysInit();

    /*
    * Activates the serial driver 2 using the driver default configuration.
    * PA2(TX) and PA3(RX) are routed to USART2.
    */
    sdStart(&SD2, NULL);
    chprintf((BaseSequentialStream *)&SD2, "Ahoy, i'm alive \r\n");

    bool sd_init_succeed = sd_init(&MMCD1, &mmccfg);

    FRESULT err;

    char b[1000];
    char c[12] = "/oi123.txt";
    // c[0] = 0;
    b[0] = 0;

    FIL fp;
    uint32_t bw;
    err = f_open(&fp, c, FA_CREATE_NEW | FA_WRITE);
    f_write(&fp, (void *)b, sizeof(b), (UINT *)&bw);
    chprintf((BaseSequentialStream *)&SD2, "open = %d, written %d\r\n", err, bw);
    f_close(&fp);

    scan_files((BaseSequentialStream *)&SD2, (char *)&b);

    chprintf((BaseSequentialStream *)&SD2, "Thread was terminated");

    while (true) {
        chThdSleepMilliseconds(500);
    }
}

/*
* SD initialization.
*/
bool sd_init(MMCDriver *mmcd, MMCConfig *mmcconfig) {
    mmc_start_with_spi(mmcd, mmcconfig);

    /*
    * On insertion SDC initialization and FS mount.
    */
    chprintf((BaseSequentialStream *)&SD2, "Trying to connect\r\n");
    while (mmcConnect(mmcd)) {
        chprintf((BaseSequentialStream *)&SD2, "Card not connected\r\n");
        chThdSleepMilliseconds(1000);
    }
    chprintf((BaseSequentialStream *)&SD2, "Connected?\r\n");

    BYTE mount_succeeded = sd_mount(&SDC_FS, mmcd);
    if (mount_succeeded) {
        return true;
    } else {
        chprintf((BaseSequentialStream *)&SD2, "Card isn't FAT formatted\r\n");
        return false;
    }
}

void mmc_start_with_spi(MMCDriver *mmcd, MMCConfig *cfg) {
    spi_pin_config();
    mmcObjectInit(mmcd);
    mmcStart(mmcd, cfg);
}

void spi_pin_config(void) {
    /*
    * SPI I/O pins setup.
    */
    palSetPadMode(SPI_SCK_PORT, SPI_SCK_PIN, PAL_MODE_STM32_ALTERNATE_PUSHPULL);   // SCK
    palSetPadMode(SPI_MISO_PORT, SPI_MISO_PIN, PAL_MODE_STM32_ALTERNATE_PUSHPULL); // MISO
    palSetPadMode(SPI_MOSI_PORT, SPI_MOSI_PIN, PAL_MODE_STM32_ALTERNATE_PUSHPULL); // MOSI
    palSetPadMode(SPI_CS_PORT, SPI_CS_PIN, PAL_MODE_OUTPUT_PUSHPULL);              // CS
    palSetPad(SPI_CS_PORT, SPI_CS_PIN);                                            // Enable CS
}

BYTE sd_mount(FATFS *fs, MMCDriver *mmcd) {
    FRESULT err;

    err = f_mount(fs, "/", 1);
    if (err != FR_OK) {
        chprintf((BaseSequentialStream *)&SD2, "Disconnecting\r\n");
        mmcDisconnect(mmcd);
    } else {
        chprintf((BaseSequentialStream *)&SD2, "TO suave\r\n");
    }

    return is_fs_ready(fs);
}

static FRESULT scan_files(BaseSequentialStream *chp, char *path) {
    static FILINFO fno;
    FRESULT res;
    DIR dir;
    size_t i;
    char *fn;

    chprintf(chp, "scanning files\r\n");
    chThdSleepMilliseconds(1000);

    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        i = strlen(path);
        while (((res = f_readdir(&dir, &fno)) == FR_OK) && fno.fname[0]) {
            if (FF_FS_RPATH && fno.fname[0] == '.') {
                chprintf(chp, "first if\r\n");
                continue;
            }

            fn = fno.fname;
            if (fno.fattrib & AM_DIR) {
                *(path + i) = '/';
                strcpy(path + i + 1, fn);
                chprintf(chp, "before recursive call\r\n");
                chThdSleepMilliseconds(200);
                res = scan_files(chp, path);
                chprintf(chp, "after recursive call\r\n");
                chThdSleepMilliseconds(200);
                
                *(path + i) = '\0';
                if (res != FR_OK)
                    break;
            } else {
                chprintf(chp, "%s/%s\r\n", path, fn);
            }
        }
    }
    return res;
}