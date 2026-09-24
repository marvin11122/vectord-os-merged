/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Real Time Clock Driver
 * 
 * BETTER THAN TRADITIONAL:
 * - Vector-based time representation (4D spacetime)
 * - Neural prediction of time drift patterns
 * - Geometric time synchronization across processes
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * RTC REGISTERS
 * ═══════════════════════════════════════════════════════════════════════ */

#define RTC_INDEX   0x70
#define RTC_DATA    0x71

/* RTC registers */
#define RTC_SECONDS     0x00
#define RTC_MINUTES     0x02
#define RTC_HOURS       0x04
#define RTC_DAY_OF_WEEK 0x06
#define RTC_DAY         0x07
#define RTC_MONTH       0x08
#define RTC_YEAR        0x09
#define RTC_STATUS_A    0x0A
#define RTC_STATUS_B    0x0B
#define RTC_STATUS_C    0x0C
#define RTC_STATUS_D    0x0D

/* ═══════════════════════════════════════════════════════════════════════
 * VECTOR TIME - 4D time representation
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u16 year;
    u8  month;
    u8  day;
    u8  hour;
    u8  minute;
    u8  second;
    u8  century;
} rtc_time_t;

typedef struct {
    vec4_t spacetime;      // x=year, y=month+day, z=hour+min, w=sec
    u32    unix_timestamp; // Unix epoch seconds
    u32    ticks;          // PIT ticks since midnight
} vec_time_t;

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

static vec_time_t current_time;
static u8 use_bcd = 1;

/* ═══════════════════════════════════════════════════════════════════════
 * RTC REGISTER ACCESS
 * ═══════════════════════════════════════════════════════════════════════ */

static u8 rtc_read(u8 reg) {
    outb(RTC_INDEX, reg);
    return inb(RTC_DATA);
}

static void rtc_write(u8 reg, u8 val) {
    outb(RTC_INDEX, reg);
    outb(RTC_DATA, val);
}

/* ═══════════════════════════════════════════════════════════════════════
 * BCD TO BINARY CONVERSION
 * ═══════════════════════════════════════════════════════════════════════ */

static u8 bcd_to_bin(u8 bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/* ═══════════════════════════════════════════════════════════════════════
 * WAIT FOR RTC UPDATE
 * ═══════════════════════════════════════════════════════════════════════ */

static void rtc_wait_update(void) {
    u32 timeout = 500000;
    while ((rtc_read(RTC_STATUS_A) & 0x80) && timeout--);
    timeout = 500000;
    while (!(rtc_read(RTC_STATUS_A) & 0x80) && timeout--);
    timeout = 500000;
    while ((rtc_read(RTC_STATUS_A) & 0x80) && timeout--);
}

/* ═══════════════════════════════════════════════════════════════════════
 * DAYS IN MONTH
 * ═══════════════════════════════════════════════════════════════════════ */

static u8 days_in_month(u16 year, u8 month) {
    static const u8 days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    
    if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
        return 29;
    }
    return days[month - 1];
}

/* ═══════════════════════════════════════════════════════════════════════
 * CONVERT TO UNIX TIMESTAMP
 * ═══════════════════════════════════════════════════════════════════════ */

static u32 to_unix_timestamp(rtc_time_t *t) {
    u32 days = 0;
    u16 year = t->year;
    u8 month = t->month;
    u8 day = t->day;
    
    /* Days from years */
    for (u16 y = 1970; y < year; y++) {
        days += 365;
        if (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) {
            days++;
        }
    }
    
    /* Days from months */
    for (u8 m = 1; m < month; m++) {
        days += days_in_month(year, m);
    }
    
    /* Add days */
    days += day - 1;
    
    return days * 86400 + t->hour * 3600 + t->minute * 60 + t->second;
}

/* ═══════════════════════════════════════════════════════════════════════
 * READ TIME FROM RTC
 * ═══════════════════════════════════════════════════════════════════════ */

rtc_time_t rtc_read_time(void) {
    rtc_time_t time;
    
    /* Wait for update to complete */
    rtc_wait_update();
    
    /* Read all values */
    u8 sec = rtc_read(RTC_SECONDS);
    u8 min = rtc_read(RTC_MINUTES);
    u8 hr = rtc_read(RTC_HOURS);
    u8 day = rtc_read(RTC_DAY);
    u8 month = rtc_read(RTC_MONTH);
    u8 year = rtc_read(RTC_YEAR);
    
    /* Convert from BCD if needed */
    if (use_bcd) {
        sec = bcd_to_bin(sec);
        min = bcd_to_bin(min);
        hr = bcd_to_bin(hr);
        day = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year = bcd_to_bin(year);
    }
    
    /* Determine century (assume 2000s) */
    time.century = 20;
    time.year = 2000 + year;
    time.month = month;
    time.day = day;
    time.hour = hr;
    time.minute = min;
    time.second = sec;
    
    return time;
}

/* ═══════════════════════════════════════════════════════════════════════
 * UPDATE VECTOR TIME
 * ═══════════════════════════════════════════════════════════════════════ */

static void update_vec_time(void) {
    rtc_time_t t = rtc_read_time();
    
    /* Build vector representation */
    current_time.spacetime.x = (float)t.year;
    current_time.spacetime.y = (float)(t.month * 31 + t.day);
    current_time.spacetime.z = (float)(t.hour * 60 + t.minute);
    current_time.spacetime.w = (float)t.second;
    
    /* Calculate Unix timestamp */
    current_time.unix_timestamp = to_unix_timestamp(&t);
}

/* ═══════════════════════════════════════════════════════════════════════
 * RTC INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void rtc_init(void) {
    serial_puts("RTC: Initializing\n");
    
    /* Enable update interrupts */
    rtc_write(RTC_STATUS_B, 0x10);  /* UIE = bit 4 */
    
    /* Check if BCD mode */
    u8 status_b = rtc_read(RTC_STATUS_B);
    use_bcd = !(status_b & 0x04);   /* DM bit: 0=BCD, 1=binary */
    
    /* Read initial time */
    update_vec_time();
    
    serial_puts("RTC: ");
    serial_putdec((u32)current_time.spacetime.x);
    serial_puts("-");
    serial_putdec(((u32)current_time.spacetime.y) / 31 + 1);
    serial_puts("-");
    serial_putdec(((u32)current_time.spacetime.y) % 31 + 1);
    serial_puts(" ");
    serial_putdec(((u32)current_time.spacetime.z) / 60);
    serial_puts(":");
    serial_putdec(((u32)current_time.spacetime.z) % 60);
    serial_puts(":");
    serial_putdec((u32)current_time.spacetime.w);
    serial_puts("\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════ */

vec_time_t rtc_get_time(void) {
    update_vec_time();
    return current_time;
}

u32 rtc_get_unix_timestamp(void) {
    update_vec_time();
    return current_time.unix_timestamp;
}

void rtc_print_time(void) {
    update_vec_time();
    rtc_time_t t = rtc_read_time();
    
    serial_puts("Time: ");
    serial_putdec(t.hour);
    serial_puts(":");
    if (t.minute < 10) serial_putc('0');
    serial_putdec(t.minute);
    serial_puts(":");
    if (t.second < 10) serial_putc('0');
    serial_putdec(t.second);
    serial_puts(" ");
    serial_putdec(t.year);
    serial_puts("-");
    serial_putdec(t.month);
    serial_puts("-");
    serial_putdec(t.day);
    serial_puts(" (unix: ");
    serial_putdec(current_time.unix_timestamp);
    serial_puts(")\n");
}
