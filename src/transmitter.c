/**
 * @file transmitter.c
 * @brief RF Transmitter — Fire Fighting Robotic Vehicle Controller
 *
 * Reads 9 active-LOW push buttons on PORTB (F0–F7) and PORTA (F0),
 * and transmits a framed single-character command over UART at 2400 baud
 * to the RF transmitter module (TWS-433 MHz).
 *
 * Target:    PIC16F877A
 * Compiler:  MikroC Pro for PIC
 * Crystal:   8 MHz
 * Baud rate: 2400
 *
 * ── Button / Command Map ──────────────────────────────────────────
 *   PORTB.F0 → (A) Base Forward
 *   PORTB.F1 → (B) Base Reverse
 *   PORTB.F2 → (C) Base Turn Left
 *   PORTB.F3 → (D) Base Turn Right
 *   PORTB.F4 → (E) Arm Up
 *   PORTB.F5 → (F) Arm Down
 *   PORTB.F6 → (G) Pump Speed Up
 *   PORTB.F7 → (H) Pump Speed Down
 *   PORTA.F0 → (M) Stop Base
 *
 * ── Frame Format ─────────────────────────────────────────────────
 *   ,,,,,,,,,,,( X ),,,,,,,,,,,
 *   Comma preamble/postamble allows the RF receiver to lock onto
 *   the carrier before the data character arrives.
 *
 * ── Wiring ───────────────────────────────────────────────────────
 *   PORTB pins 0–7 : push buttons to GND (active-LOW, 10k pull-up)
 *   PORTA pin 0    : push button to GND  (active-LOW, 10k pull-up)
 *   PORTC.F6 (TX)  : connected to DATA pin of TWS-433 TX module
 */

/* ── Globals ──────────────────────────────────────────────────── */
char tx_buf[16];
int  tx_i = 0;

/* ── Macro: transmit one framed command character ─────────────── */
#define SEND_CMD(ch)                    \
    do {                                \
        UARt1_Write_Text(",,,,,,,,,,,"); \
        UART1_Write('(');               \
        UART1_Write(ch);                \
        UART1_Write(')');               \
        UARt1_Write_Text(",,,,,,,,,,,"); \
    } while (0)

/* ── Main ─────────────────────────────────────────────────────── */
void main(void)
{
    /* Configure all PORTA/PORTB pins as digital I/O (no ADC) */
    ADCON1 = 0x06;

    /* Clear text buffer */
    for (tx_i = 0; tx_i < 16; tx_i++) { tx_buf[tx_i] = ' '; }

    TRISB = 0xFF;       /* PORTB: all inputs (buttons) */
    TRISA = 0xFF;       /* PORTA: all inputs (buttons) */

    UART1_Init(2400);
    delay_ms(200);      /* Allow UART to stabilise */

    while (1)
    {
        /* Each button is active-LOW with 100 ms software debounce.
         * UART1_Tx_Idle() ensures we don't overlap transmissions. */

        if (PORTB.F0 == 0) { delay_ms(100); if (UART1_Tx_Idle()) SEND_CMD('A'); } /* Base Forward    */
        if (PORTB.F1 == 0) { delay_ms(100); if (UART1_Tx_Idle()) SEND_CMD('B'); } /* Base Reverse    */
        if (PORTB.F2 == 0) { delay_ms(100); if (UART1_Tx_Idle()) SEND_CMD('C'); } /* Turn Left       */
        if (PORTB.F3 == 0) { delay_ms(100); if (UART1_Tx_Idle()) SEND_CMD('D'); } /* Turn Right      */
        if (PORTB.F4 == 0) { delay_ms(100); if (UART1_Tx_Idle()) SEND_CMD('E'); } /* Arm Up          */
        if (PORTB.F5 == 0) { delay_ms(100); if (UART1_Tx_Idle()) SEND_CMD('F'); } /* Arm Down        */
        if (PORTB.F6 == 0) { delay_ms(100); if (UART1_Tx_Idle()) SEND_CMD('G'); } /* Pump Speed Up   */
        if (PORTB.F7 == 0) { delay_ms(100); if (UART1_Tx_Idle()) SEND_CMD('H'); } /* Pump Speed Down */
        if (PORTA.F0 == 0) { delay_ms(100); if (UART1_Tx_Idle()) SEND_CMD('M'); } /* Stop Base       */
    }
}
