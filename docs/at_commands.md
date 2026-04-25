# AT Command Reference

This document lists the AT commands used in `gsm_feedback.c` and additional commands useful for debugging the GSM module manually (e.g. via a serial terminal at 9600 baud).

---

## Commands Used in Firmware

| Command | Description |
|---|---|
| `AT` | Ping the modem — responds `OK` if alive |
| `ATE0` | Disable echo (modem won't repeat your commands back) |
| `AT+CMGF=1` | Set SMS mode to **text** (as opposed to PDU/binary) |
| `AT+CMGS="<number>"` | Open an outgoing SMS to `<number>`; modem responds with `>` prompt |
| `0x1A` (Ctrl-Z) | Terminate and send the SMS body |

---

## Useful Debugging Commands

### General

| Command | Description |
|---|---|
| `ATI` | Display modem model/firmware info |
| `AT+CPIN?` | Check SIM card status (`READY`, `SIM PIN`, etc.) |
| `AT+CSQ` | Signal quality (`+CSQ: <rssi>,<ber>`) |
| `AT+CREG?` | Network registration status |

### SMS

| Command | Example | Description |
|---|---|---|
| `AT+CMGF=1` | — | Text mode |
| `AT+CMGL="ALL"` | — | List all SMS messages |
| `AT+CMGR=<n>` | `AT+CMGR=1` | Read SMS at index `n` |
| `AT+CMGD=<n>` | `AT+CMGD=1` | Delete SMS at index `n` |
| `AT+CMGS="<num>"` | `AT+CMGS="+251920950788"` | Send SMS (follow with body + Ctrl-Z) |

### Calls

| Command | Example | Description |
|---|---|---|
| `ATD<number>;` | `ATD+251920950788;` | Dial voice call (semicolon = voice) |
| `ATDL` | — | Redial last number |
| `ATA` | — | Answer incoming call |
| `ATH` | — | Hang up / disconnect |

### Echo

| Command | Description |
|---|---|
| `ATE0` | Echo off |
| `ATE1` | Echo on |

---

## Example SMS Session (manual terminal)

```
AT
OK
ATE0
OK
AT+CMGF=1
OK
AT+CMGS="+251920950788"
> temperature=27obstacle from=left front<Ctrl-Z>
+CMGS: 1
OK
```

---

## Notes

- All commands are terminated with `\r` (carriage return, `0x0D`) in firmware.
- The SMS body is terminated with `0x1A` (Ctrl-Z), not `\r\n`.
- If the modem returns `ERROR`, check: SIM inserted, antenna connected, network registered (`AT+CREG?`).
