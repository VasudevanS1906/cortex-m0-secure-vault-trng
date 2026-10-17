#include "stm32c0xx.h"

// 128-Bit Secret Key in SRAM (Wiped to 0x00000000 when Red Button is clicked!)
volatile uint32_t crypto_key[4] = { 0xA56BABCD, 0x0000FFFF, 0x12345678, 0x9ABCDEF0 };
volatile uint8_t  tamper_detected = 0;
uint32_t packet_counter = 0;

__attribute__((aligned(256))) uint32_t ram_vector_table[48];

void uart2_write_char(char ch) {
    while (!(USART2->ISR & USART_ISR_TXE_TXFNF)) {}
    USART2->TDR = ch;
}

void uart2_print(const char *str) {
    while (*str) uart2_write_char(*str++);
}

void uart2_print_uint(uint32_t num) {
    char buf[12];
    int i = 0;
    if (num == 0) { uart2_write_char('0'); return; }
    while (num > 0) { buf[i++] = (num % 10) + '0'; num /= 10; }
    while (i > 0) uart2_write_char(buf[--i]);
}

void uart2_print_hex32(uint32_t val) {
    const char hex[] = "0123456789ABCDEF";
    uart2_print("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uart2_write_char(hex[(val >> i) & 0xF]);
    }
}

// HARDWARE TAMPER INTERRUPT: Wipes Secret Key in RAM when Red Button is pressed
void My_Tamper_Zeroize_ISR(void) {
    if (EXTI->FPR1 & (1UL << 0)) {
        EXTI->FPR1 = (1UL << 0);
        if (!tamper_detected) {
            for (int i = 0; i < 4; i++) crypto_key[i] = 0x00000000;
            GPIOA->BSRR = (1UL << (5 + 16));
            tamper_detected = 1;
        } else {
            crypto_key[0] = 0xA56BABCD; crypto_key[1] = 0x0000FFFF;
            crypto_key[2] = 0x12345678; crypto_key[3] = 0x9ABCDEF0;
            tamper_detected = 0;
        }
    }
}

// STM32 HARDWARE CRC-32 HASH ACCELERATOR (Uses Silicon CRC->DR Register)
uint32_t hw_crc32_compute(uint32_t word0, uint32_t word1) {
    CRC->CR = CRC_CR_RESET;
    CRC->DR = word0;
    CRC->DR = word1;
    return CRC->DR;
}

// 128-Bit Key Block Cipher (32-Round Feistel Network)
void encrypt_64bit_block(uint32_t v[2], volatile uint32_t const key[4]) {
    uint32_t v0 = v[0], v1 = v[1], sum = 0, delta = 0x9E3779B9;
    for (uint32_t i = 0; i < 32; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
        sum += delta;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
    }
    v[0] = v0; v[1] = v1;
}

void decrypt_64bit_block(uint32_t v[2], volatile uint32_t const key[4]) {
    uint32_t v0 = v[0], v1 = v[1], delta = 0x9E3779B9, sum = delta * 32;
    for (uint32_t i = 0; i < 32; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    }
    v[0] = v0; v[1] = v1;
}

uint16_t adc1_read_pa1(void) {
    ADC1->ISR |= ADC_ISR_EOC;             // Clear old EOC flag
    ADC1->CR  |= ADC_CR_ADSTART;          // Start conversion
    uint32_t timeout = 10000;
    while (!(ADC1->ISR & ADC_ISR_EOC) && --timeout) {}
    return (uint16_t)ADC1->DR;
}

int main(void) {
    uint32_t *flash_vectors = (uint32_t *)SCB->VTOR;
    for (int i = 0; i < 48; i++) ram_vector_table[i] = flash_vectors[i];
    ram_vector_table[16 + EXTI0_1_IRQn] = (uint32_t)&My_Tamper_Zeroize_ISR;
    SCB->VTOR = (uint32_t)ram_vector_table;

    // Enable Clocks: GPIOA, CRC-32 Hardware Engine, TIM3, USART2, ADC
    RCC->IOPENR  |= RCC_IOPENR_GPIOAEN;
    RCC->AHBENR  |= RCC_AHBENR_CRCEN;
    RCC->APBENR1 |= RCC_APBENR1_TIM3EN | RCC_APBENR1_USART2EN;
    RCC->APBENR2 |= RCC_APBENR2_ADCEN;

    // PA5 = Output, PA0 = Pull-Up Button, PA1 = Analog Knob
    GPIOA->MODER &= ~((3UL << (5 * 2)) | (3UL << (0 * 2)) | (3UL << (1 * 2)));
    GPIOA->MODER |=  (1UL << (5 * 2)) | (3UL << (1 * 2));
    GPIOA->PUPDR &= ~(3UL << (0 * 2));
    GPIOA->PUPDR |=  (1UL << (0 * 2));

    EXTI->EXTICR[0] &= ~(0xFFUL << 0);
    EXTI->FTSR1     |= (1UL << 0);
    EXTI->IMR1      |= (1UL << 0);
    NVIC_EnableIRQ(EXTI0_1_IRQn);

    GPIOA->MODER  &= ~(3UL << (2 * 2));
    GPIOA->MODER  |=  (2UL << (2 * 2));
    GPIOA->AFR[0] &= ~(0xFUL << (2 * 4));
    GPIOA->AFR[0] |=  (1UL << (2 * 4));
    USART2->BRR = 417;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;

    // Complete STM32C0 ADC1 Hardware Calibration & Ready Sequence
    if (ADC1->CR & ADC_CR_ADEN) {
        ADC1->CR |= ADC_CR_ADDIS;
        while (ADC1->CR & ADC_CR_ADEN) {}
    }
    ADC1->CFGR2 |= (1UL << ADC_CFGR2_CKMODE_Pos);
    ADC1->CR    |= ADC_CR_ADVREGEN;
    for (volatile int i = 0; i < 2000; i++);
    ADC1->CR    |= ADC_CR_ADCAL;                  // Run Hardware Calibration
    while (ADC1->CR & ADC_CR_ADCAL) {}
    ADC1->ISR   |= ADC_ISR_ADRDY;
    ADC1->CR    |= ADC_CR_ADEN;                   // Enable ADC1
    while (!(ADC1->ISR & ADC_ISR_ADRDY)) {}       // Wait for ADC Ready
    ADC1->CHSELR = (1UL << 1);                    // Select Channel 1 (PA1)

    TIM3->PSC = 47999;
    TIM3->ARR = 0xFFFF;
    TIM3->CR1 |= TIM_CR1_CEN;

    uart2_print("\r\n=== STM32 HARDWARE CRYPTO VAULT & SILICON CRC-32 ENGINE ===\r\n");
    uart2_print("Turn Knob = Change Sensor | Click Red Button = TAMPER KEY WIPE!\r\n\r\n");

    while (1) {
        if (EXTI->FPR1 & (1UL << 0)) My_Tamper_Zeroize_ISR();

        if (tamper_detected) {
            GPIOA->BSRR = (1UL << (5 + 16));
            uart2_print("[TAMPER ALARM!] Key Zeroized in RAM: ");
            uart2_print_hex32(crypto_key[0]);
            uart2_print(" | Vault Locked! (Click Red Button to Re-Key)\r\n");
        } else {
            GPIOA->BSRR ^= (1UL << 5);
            uint16_t sensor_mv = (adc1_read_pa1() * 3300UL) / 4095UL;
            packet_counter++;

            uint32_t payload[2] = { packet_counter, sensor_mv };

            // 1. Hardware CRC-32 Hash in Silicon
            uint32_t tx_crc_hash = hw_crc32_compute(payload[0], payload[1]);

            // 2. Encrypt 64-bit Block using 128-Bit Key
            encrypt_64bit_block(payload, crypto_key);
            uint32_t cipher0 = payload[0], cipher1 = payload[1];

            // 3. Decrypt & Verify Integrity with Hardware CRC-32 Unit
            decrypt_64bit_block(payload, crypto_key);
            uint32_t rx_crc_hash = hw_crc32_compute(payload[0], payload[1]);

            uart2_print("Pkt #");
            uart2_print_uint(packet_counter);
            uart2_print(" | Plain: ");
            uart2_print_uint(sensor_mv);
            uart2_print("mV | Ciphertext: ");
            uart2_print_hex32(cipher0);
            uart2_write_char(' ');
            uart2_print_hex32(cipher1);
            uart2_print(" | HW-CRC32: ");
            uart2_print_hex32(tx_crc_hash);
            if (tx_crc_hash == rx_crc_hash) {
                uart2_print(" [VERIFIED]\r\n");
            }
        }

        TIM3->CNT = 0;
        while (TIM3->CNT < 800) {
            if (EXTI->FPR1 & (1UL << 0)) My_Tamper_Zeroize_ISR();
        }
    }
}
