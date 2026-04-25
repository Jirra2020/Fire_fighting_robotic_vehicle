/**
 * @file receiver.c
 * @brief RF Receiver — Motor Driver, Servo Arm & Water Pump Controller
 *
 * Receives framed UART commands from the RF receiver module (TWR-433 MHz)
 * and controls:
 *   1. Base DC motors  — L293D H-bridge on PORTB (Hardware PWM1 speed)
 *   2. Arm servo motor — Hardware PWM2 at 50 Hz  (CCP2 / PORTC.F1)
 *   3. Water pump      — Software PWM             (PORTC.F0)
 *
 * Target:    PIC16F877A
 * Compiler:  MikroC Pro for PIC
 * Crystal:   8 MHz
 * Baud rate: 2400
 *
 * ── Pin Assignments ───────────────────────────────────────────────
 *   PORTB 0–7       : L293D H-bridge control inputs  (base motors)
 *   PORTC.F2 (CCP1) : Hardware PWM1 → L293D EN pin   (base speed)
 *   PORTC.F1 (CCP2) : Hardware PWM2 → Servo signal   (arm)
 *   PORTC.F0        : Software PWM  → Pump MOSFET/relay gate
 *   PORTC.F7 (RX)   : UART1 input   → TWR-433 DATA pin
 *
 * ── Base Motor PORTB Truth Table (L293D, two motors) ─────────────
 *   PORTB = 0x05  (0000 0101) — Forward-left  (Motor A fwd, Motor B fwd)
 *   PORTB = 0x0A  (0000 1010) — Forward-right (Motor A rev, Motor B rev)
 *   PORTB = 0x02  (0000 0010) — Forward       (Motor A fwd only)
 *   PORTB = 0x04  (0000 0100) — Reverse       (Motor B fwd only)
 *   PORTB = 0x50  (0101 0000) — Rotate Left
 *   PORTB = 0x60  (0110 0000) — Rotate Right
 *   PORTB = 0x00  (0000 0000) — Stop
 *
 * ── Servo PWM2 Duty Counts (50 Hz, 8 MHz crystal) ────────────────
 *   Duty 52  ≈ 1.0 ms pulse → 0°   (arm fully down)
 *   Duty 154 ≈ 1.5 ms pulse → 90°  (arm centre / home position)
 *   Duty 256 ≈ 2.0 ms pulse → 180° (arm fully up)
 *   Step per button press = 10 counts ≈ 18° movement
 *
 * ── Water Pump Software PWM (PORTC.F0) ───────────────────────────
 *   pump_duty = 0   → pump OFF
 *   pump_duty = 255 → pump full speed
 *   Step per G/H button press = 25 counts (10 steps total)
 */

/* ── Constants ────────────────────────────────────────────────── */

/* Base motor speed — Hardware PWM1 on CCP1 (PORTC.F2) */
#define BASE_DUTY_INIT   35     /* Starting duty on power-up        */
#define BASE_DUTY_MAX   255     /* Maximum duty cycle               */
#define BASE_DUTY_MIN    35     /* Minimum duty — motor just starts */

/* Servo arm — Hardware PWM2 on CCP2 (PORTC.F1) at 50 Hz */
#define SERVO_HOME      154     /* 90°  centre position (power-up)  */
#define SERVO_MAX       256     /* 180° arm fully up                */
#define SERVO_MIN        52     /* 0°   arm fully down              */
#define SERVO_STEP       10     /* ~18° per E/F button press        */

/* Water pump — Software PWM on PORTC.F0 */
#define PUMP_DUTY_INIT    0     /* Pump OFF at power-up             */
#define PUMP_DUTY_MAX   255     /* Full speed                       */
#define PUMP_DUTY_MIN     0     /* Off                              */
#define PUMP_DUTY_STEP   25     /* Step per G/H button press        */

/* ── Globals ──────────────────────────────────────────────────── */
char rx_buf[16];
char rx_chk;
int  rx_i      = 0;

int  base_duty = BASE_DUTY_INIT;
int  servo_pos = SERVO_HOME;
int  pump_duty = PUMP_DUTY_INIT;

/* ── Software PWM for water pump (PORTC.F0) ──────────────────── */
/*
 * Generates one ~1 ms Software PWM cycle on PORTC.F0.
 * Must be called continuously in the main loop to sustain the signal.
 *
 *   pump_duty = 0   → stays LOW  entire cycle (pump off)
 *   pump_duty = 255 → stays HIGH entire cycle (full speed)
 *   Values in between → proportional ON/OFF time within the cycle
 */
void soft_pwm_pump(void)
{
    if (pump_duty > PUMP_DUTY_MIN)
    {
        PORTC.F0 = 1;
        Delay_us(pump_duty * 4);            /* ON  time */
    }
    if (pump_duty < PUMP_DUTY_MAX)
    {
        PORTC.F0 = 0;
        Delay_us((255 - pump_duty) * 4);    /* OFF time */
    }
}

/* ── Stop base motors only ────────────────────────────────────── */
/*
 * Zeroes PORTB (disables H-bridge) and resets base PWM duty.
 * Arm and pump are unaffected — per project design spec.
 */
void stop_base(void)
{
    PORTB     = 0x00;
    base_duty = BASE_DUTY_INIT;
    PWM1_Set_Duty(base_duty);
}

/* ── Main ─────────────────────────────────────────────────────── */
void main(void)
{
    int i;

    /*
     * ADCON1 = 0x06: configure all PORTA and PORTE pins as digital
     * I/O so PORTC PWM output pins (CCP1, CCP2) are unaffected
     * by the ADC module.
     */
    ADCON1 = 0x06;

    TRISB = 0x00;       /* PORTB: all outputs → L293D H-bridge inputs  */
    TRISC = 0x80;       /* PORTC: RC7 input (UART RX), all others output */

    /* Clear receive buffer */
    for (i = 0; i < 16; i++) { rx_buf[i] = ' '; }

    /* UART — connected to TWR-433 RF receiver DATA output */
    UART1_Init(2400);
    delay_ms(200);

    /*
     * Hardware PWM1 — base motor speed via L293D enable pin
     * Frequency: 1 kHz    Pin: CCP1 / PORTC.F2
     */
    PWM1_Init(1000);
    PWM1_Start();
    PWM1_Set_Duty(base_duty);

    /*
     * Hardware PWM2 — servo arm control
     * Frequency: 50 Hz (standard servo period = 20 ms)
     * Pin: CCP2 / PORTC.F1
     * Starts at home position (90°)
     */
    PWM2_Init(50);
    PWM2_Start();
    PWM2_Set_Duty(servo_pos);

    /* Safe initial state */
    PORTB    = 0x00;    /* Motors off          */
    PORTC.F0 = 0;       /* Pump off            */

    while (1)
    {
        /* ── Sustain water pump Software PWM ──────────────────── */
        soft_pwm_pump();

        /* ── Process incoming RF command ──────────────────────── */
        if (UART1_Data_Ready() == 1)
        {
            rx_chk = UART1_Read();

            if (rx_chk == '(')          /* Start-of-frame marker */
            {
                /* Clear buffer before reading new command */
                for (rx_i = 0; rx_i < 16; rx_i++) { rx_buf[rx_i] = ' '; }
                rx_chk = ' ';

                /* Read command char up to end-of-frame ')'; max 2 chars */
                UART1_Read_Text(rx_buf, ")", 2);

                switch (rx_buf[0])
                {
                    /* ── Base movement (PORTB patterns from thesis) ── */

                    case 'A':                       /* Forward-left     */
                        PORTB = 0x05;
                        PWM1_Set_Duty(base_duty);
                        break;

                    case 'B':                       /* Forward-right    */
                        PORTB = 0x0A;
                        PWM1_Set_Duty(base_duty);
                        break;

                    case 'C':                       /* Forward          */
                        PORTB = 0x02;
                        PWM1_Set_Duty(base_duty);
                        break;

                    case 'D':                       /* Reverse          */
                        PORTB = 0x04;
                        PWM1_Set_Duty(base_duty);
                        break;

                    case 'M':                       /* Stop base only   */
                        stop_base();
                        break;

                    /* ── Servo arm ─────────────────────────────────── */

                    case 'E':                       /* Arm Up           */
                        if (servo_pos < SERVO_MAX) {
                            servo_pos += SERVO_STEP;
                            if (servo_pos > SERVO_MAX) servo_pos = SERVO_MAX;
                            PWM2_Set_Duty(servo_pos);
                            Delay_ms(20);           /* Servo settle time */
                        }
                        break;

                    case 'F':                       /* Arm Down         */
                        if (servo_pos > SERVO_MIN) {
                            servo_pos -= SERVO_STEP;
                            if (servo_pos < SERVO_MIN) servo_pos = SERVO_MIN;
                            PWM2_Set_Duty(servo_pos);
                            Delay_ms(20);           /* Servo settle time */
                        }
                        break;

                    /* ── Water pump speed ───────────────────────────── */

                    case 'G':                       /* Pump faster      */
                        if (pump_duty < PUMP_DUTY_MAX) {
                            pump_duty += PUMP_DUTY_STEP;
                            if (pump_duty > PUMP_DUTY_MAX) pump_duty = PUMP_DUTY_MAX;
                        }
                        break;

                    case 'H':                       /* Pump slower      */
                        if (pump_duty > PUMP_DUTY_MIN) {
                            pump_duty -= PUMP_DUTY_STEP;
                            if (pump_duty < PUMP_DUTY_MIN) pump_duty = PUMP_DUTY_MIN;
                        }
                        break;

                    default: break;                 /* Unknown — ignore */
                }
            }
        }
    }
}
