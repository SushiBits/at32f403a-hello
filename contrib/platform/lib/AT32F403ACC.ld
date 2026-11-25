/**
 * @file AT32F403ACC.ld
 */

MEMORY {
    IROM (rx)  : ORIGIN = 0x08000000, LENGTH = 256k
    IRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 96k
}