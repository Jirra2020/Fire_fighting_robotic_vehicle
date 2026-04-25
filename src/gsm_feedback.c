/**
 * @file gsm_feedback.c
 * @brief GSM Feedback — Smoke Detection, Temperature & Obstacle SMS
 *
 * Monitors an MQ5 gas/smoke sensor continuously. When smoke is detected,
 * reads temperature (LM35) and four IR obstacle sensors, then sends a
 * formatted SMS to the operator's mobile number via a SIM900D GSM module.
 * SMS is re-sent every 10 seconds for as long as smoke is present.
 *
 * Target:    PIC16F877A
 * Compiler:  MikroC Pro for PIC
 * Crystal:   8 MHz
 * Baud rate: 9600  (SIM900D requirement)
 *
 * ── Pin Assignments ───────────────────────────────────────────────
 *   PORTA.F0 (ADC ch0) : LM35 temperature sensor output
 *   PORTA.F1 (ADC ch1) : IR obstacle sensor — Left
 *   PORTA.F2 (ADC ch2) : IR obstacle sensor — Right
 *   PORTA.F3 (ADC ch3) : IR obstacle sensor — Front
 *   PORTA.F4 (ADC ch4) : IR obstacle sensor — Back
 *   PORTB.F0           : MQ5 gas/smoke sensor digital output
 *                        (HIGH = smoke detected)
 *   PORTC.F6 (TX)      : connected to SIM900D RX pin (via MAX232)
 *   PORTC.F7 (RX)      : connected to SIM900D TX pin (via MAX232)
 *
 * ── Note on MAX232 ────────────────────────────────────────────────
 *   The SIM900D operates at RS-232 voltage levels. A MAX232 level
 *   shifter is required between PORTC.F6/F7 and the SIM900D module.
 *
 * ── Obstacle Sensor (Sharp GP2Y0A700KOF) ─────────────────────────
 *   Measuring range: 100–550 cm
 *   Output voltage inversely proportional to distance.
 *   OBSTACLE_THRESHOLD (500 ADC counts ≈ 2.44 V) detects objects
 *   within approximately 150 cm of the robot.
 *
 * ── LM35 Temperature Conversion ──────────────────────────────────
 *   ADC result (10-bit, 5V ref) × 0.4887 ≈ temperature in °C
 *   (Scale factor: 5000 mV / 1023 steps ≈ 4.887 mV/step;
 *    LM35 outputs 10 mV/°C → combined factor ≈ 0.4887)
 *
 * ── SMS Trigger ───────────────────────────────────────────────────
 *   MQ5 output HIGH → smoke detected → send SMS every 10 seconds
 *   MQ5 output LOW  → no smoke → system is idle (no SMS sent)
 *
 * ── Example SMS ───────────────────────────────────────────────────
 *   "temperature=34 obstacle from=left front "
 *
 * ── Update Before Flashing ────────────────────────────────────────
 *   Change RECIPIENT_NUMBER to your own number.
 */

/* ── Configuration ────────────────────────────────────────────── */
#define OBSTACLE_THRESHOLD   500u       /* ADC counts (~2.44 V on 5 V ref) */
#define SMS_REPEAT_DELAY_MS  10000u     /* Re-send SMS every 10 seconds     */
#define RECIPIENT_NUMBER     "+251920950788"

/* ── AT command strings ───────────────────────────────────────── */
static const char GSM_AT[]        = "AT";
static const char GSM_NOECHO[]    = "ATE0";
static const char GSM_TEXT_MODE[] = "AT+CMGF=1";
static const char GSM_MOBILE[]    = "AT+CMGS=\"" RECIPIENT_NUMBER "\"";
static const char GSM_TERMINATOR  = 0x1A;   /* Ctrl-Z — terminates SMS body */

/* ── SMS body fragments ───────────────────────────────────────── */
static const char SMS_TEMP[]   = "temperature=";
static const char SMS_OBS[]    = " obstacle from=";
static const char SMS_LEFT[]   = "left ";
static const char SMS_RIGHT[]  = "right ";
static const char SMS_FRONT[]  = "front ";
static const char SMS_BACK[]   = "back ";
static const char SMS_NONE[]   = "none";    /* No obstacles detected */

/* ── Working variables ────────────────────────────────────────── */
static float gsm_temp;
static float obs_left, obs_right, obs_front, obs_back;
static char  gsm_txt[7];                /* ASCII temperature string */
static unsigned char obstacle_found;    /* Flag: at least one obstacle seen */

/* ── Read LM35 temperature from ADC ch0 ──────────────────────── */
static void read_temp(void)
{
    gsm_temp = ADC_Read(0);
    gsm_temp = gsm_temp * 0.4887f;  /* Convert ADC count → °C */
}

/* ── Convert temperature float → ASCII string ────────────────── */
static void convert_temp(void)
{
    inttostr((int)gsm_temp, gsm_txt);
}

/* ── Send AT command string followed by carriage return ──────── */
/*
 * Used for AT handshake commands (AT, ATE0, AT+CMGF=1, AT+CMGS).
 * Carriage return (0x0D) is the AT command terminator.
 */
static void gsm_send_cmd(const char *s)
{
    while (*s) { UART1_Write(*s++); }
    UART1_Write(0x0D);
}

/* ── Send raw string to modem — no terminator ────────────────── */
/*
 * Used to build the SMS body piece by piece.
 * No CR is appended; the body is closed with Ctrl-Z instead.
 */
static void gsm_send_raw(const char *s)
{
    while (*s) { UART1_Write(*s++); }
}

/* ── Read all obstacle sensors and transmit results ──────────── */
static void send_obstacle_data(void)
{
    obstacle_found = 0;

    obs_left  = ADC_Read(1);
    obs_right = ADC_Read(2);
    obs_front = ADC_Read(3);
    obs_back  = ADC_Read(4);

    if (obs_left  > OBSTACLE_THRESHOLD) { gsm_send_raw(SMS_LEFT);  delay_ms(10); obstacle_found = 1; }
    if (obs_right > OBSTACLE_THRESHOLD) { gsm_send_raw(SMS_RIGHT); delay_ms(10); obstacle_found = 1; }
    if (obs_front > OBSTACLE_THRESHOLD) { gsm_send_raw(SMS_FRONT); delay_ms(10); obstacle_found = 1; }
    if (obs_back  > OBSTACLE_THRESHOLD) { gsm_send_raw(SMS_BACK);  delay_ms(10); obstacle_found = 1; }

    /* If no obstacle in any direction, report clearly */
    if (!obstacle_found) { gsm_send_raw(SMS_NONE); delay_ms(10); }
}

/* ── Compose and send full SMS ───────────────────────────────── */
static void send_sms(void)
{
    /* Temperature segment */
    gsm_send_raw(SMS_TEMP);     delay_ms(10);
    gsm_send_raw(gsm_txt);      delay_ms(10);

    /* Obstacle segment */
    gsm_send_raw(SMS_OBS);      delay_ms(10);
    send_obstacle_data();

    /* Ctrl-Z terminates and dispatches the SMS */
    UART1_Write(GSM_TERMINATOR);
    delay_ms(1000);
}

/* ── Perform full AT handshake then send SMS ─────────────────── */
static void send_status_sms(void)
{
    /* Step 1: read sensors */
    read_temp();       delay_ms(10);
    convert_temp();    delay_ms(10);

    /* Step 2: AT command sequence */
    gsm_send_cmd(GSM_AT);           delay_ms(100);  /* Check modem alive    */
    gsm_send_cmd(GSM_NOECHO);       delay_ms(100);  /* Turn echo off        */
    gsm_send_cmd(GSM_TEXT_MODE);    delay_ms(100);  /* Set text SMS mode    */
    gsm_send_cmd(GSM_MOBILE);       delay_ms(100);  /* Set recipient number */

    /* Step 3: compose and send SMS body */
    send_sms();
}

/* ── Main ─────────────────────────────────────────────────────── */
void main(void)
{
    /*
     * ADCON1 = 0x00: all PORTA pins configured as analogue inputs
     * for the ADC (temperature + 4 obstacle sensors on ch0–ch4).
     */
    ADCON1 = 0x00;

    TRISA = 0xFF;       /* PORTA: all inputs (ADC sensors)              */
    TRISB = 0xFF;       /* PORTB: all inputs (MQ5 gas sensor on F0)     */

    ADC_Init();
    UART1_Init(9600);   /* SIM900D GSM module baud rate                 */
    delay_ms(500);      /* Allow GSM module to power up and register    */

    while (1)
    {
        /*
         * MQ5 gas sensor digital output on PORTB.F0:
         *   HIGH (1) = smoke / gas detected above threshold
         *   LOW  (0) = air is clear
         *
         * When smoke is detected, send an SMS and wait 10 seconds
         * before checking again (prevents flooding the recipient).
         */
        if (PORTB.F0 == 1)
        {
            send_status_sms();
            delay_ms(SMS_REPEAT_DELAY_MS);  /* Wait 10 s before re-checking */
        }
    }
}
