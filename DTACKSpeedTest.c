/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Greg Campbell
 *
 * DTACKSpeedTest.c -- Mac Portable DTACK speed tester
 * Native QuickDraw window; no Retro68 CONSOLE dependency.
 * Results written to DTACKResults.txt in the same folder as the app.
 *
 * Run 1: before sleep  -- note the ratio
 * Sleep and wake the machine, then press any key in the app
 * Run 2: after wake    -- compare ratio
 *
 * FAST  (ratio < 1.30x): DTACK persists / is patched
 * SLOW  (ratio > 2.00x): DTACK lost after wake
 */

#include <MacTypes.h>
#include <QuickDraw.h>
#include <Windows.h>
#include <Fonts.h>
#include <Events.h>
#include <Memory.h>
#include <OSUtils.h>
#include <Gestalt.h>
#include <LowMem.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define LOWER_RAM_BASE  0x004000L   /* built-in RAM baseline (above zero page/globals) */
#define EXPANSION_BASE  0x100000L   /* expansion card always starts here */
#define BUILTIN_SIZE    0x100000L   /* 1MB built-in RAM */
#define ONE_MB          0x100000L
#define TOP_OFFSET      0x004000L   /* test 16KB below top of card */
#define BELOW_4MB       0x3FC000L   /* 16KB below A22 transition */
#define AT_4MB          0x400000L   /* A22 asserted */
#define ABOVE_4MB       0x404000L   /* 16KB above A22 transition */
#define BUF_LONGS       4096L
#define SWEEPS          256L

#define WIN_W   500
#define WIN_H   360
#define LMARGIN 8
#define LHEIGHT 12

static WindowPtr gWin;
static int       gY;
static FILE     *gLog = NULL;

/* ---- window / text ---- */

static void mac_init(void)
{
    Rect r;
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitCursor();
    FlushEvents(everyEvent, 0);
    SetRect(&r, 10, 40, 10 + WIN_W, 40 + WIN_H);
    gWin = NewWindow(nil, &r, "\pDTACK Speed Test", true,
                     noGrowDocProc, (WindowPtr)-1L, false, 0);
    SetPort(gWin);
    TextFont(4);    /* Monaco */
    TextSize(9);
    gY = LHEIGHT + 2;
}

static void pline(const char *s)
{
    MoveTo(LMARGIN, gY);
    DrawText((Ptr)s, 0, strlen(s));
    gY += LHEIGHT;
    if (gLog) { fputs(s, gLog); fputc('\r', gLog); fflush(gLog); }
}

static void pfmt(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    pline(buf);
}

static void pblank(void)
{
    gY += LHEIGHT;
    if (gLog) { fputc('\r', gLog); fflush(gLog); }
}

static void clear_window(void)
{
    Rect r;

    SetPort(gWin);
    r = gWin->portRect;
    EraseRect(&r);
    gY = LHEIGHT + 2;
}

/* ---- input ---- */

static char wait_key(void)
{
    EventRecord evt;
    for (;;) {
        WaitNextEvent(everyEvent, &evt, 30L, nil);
        if (evt.what == updateEvt) {
            BeginUpdate(gWin);
            EndUpdate(gWin);
        }
        if (evt.what == keyDown || evt.what == autoKey)
            return (char)(evt.message & charCodeMask);
    }
}

/* ---- speed test ---- */

static void do_reads(volatile long *p, long n)
{
    while (n--) (void)*p++;
}

static long timed_test(volatile long *base, long sweeps)
{
    long i, t0, t1;
    do_reads(base, BUF_LONGS);
    t0 = TickCount();
    for (i = sweeps; i > 0; i--) do_reads(base, BUF_LONGS);
    t1 = TickCount();
    return t1 - t0;
}

static long best_timed_test(volatile long *base, long sweeps)
{
    long best = 0x7FFFFFFFL;
    long result;
    int trial;

    for (trial = 0; trial < 3; trial++) {
        result = timed_test(base, sweeps);
        if (result < best)
            best = result;
    }

    return best;
}

static long physical_ram(void)
{
    long val;

    if (Gestalt(gestaltPhysicalRAMSize, &val) == noErr)
        return val;

    val = (long)LMGetMemTop();

    /*
     * Portable-specific fallback. Recognize the expected installed
     * configurations rather than treating MemTop as an exact size.
     */
    if (val > 0x700000L)
        return 0x900000L;

    if (val > 0x300000L)
        return 0x500000L;

    return 0x100000L;
}

/* ---- one run ---- */

static void test_region(volatile long *hiBuf, long loT, const char *label)
{
    long hiT, r100, rW, rF;
    hiT  = best_timed_test(hiBuf, SWEEPS);
    r100 = (loT > 0L)
         ? ((hiT / loT) * 100L) + (((hiT % loT) * 100L) / loT)
         : 10000L;
    rW   = r100 / 100L;
    rF   = r100 % 100L;
    pfmt("  %s: %ld ticks  %ld.%02ldx  %s",
         label, hiT, rW, rF,
         r100 < 130L ? "FAST" : r100 < 200L ? "MARGINAL" : "SLOW");
}


static void run_test(int runNum, long ram_size)
{
    long loT, card_size, top_addr, addr;
    char label[12];
    int  i, n_mb;
    volatile long *lo = (volatile long *)LOWER_RAM_BASE;

    card_size = ram_size - BUILTIN_SIZE;
    n_mb      = (int)(card_size / ONE_MB);
    top_addr  = EXPANSION_BASE + card_size - TOP_OFFSET;

    pfmt("--- Run %d -----------------------------------------", runNum);

    loT = best_timed_test(lo, SWEEPS);
    pfmt("  $004000 (built-in) : %ld ticks (baseline)", loT);

    for (i = 0; i < n_mb; i++) {
        addr = EXPANSION_BASE + (long)i * ONE_MB;
        sprintf(label, "$%06lX", addr);
        test_region((volatile long *)addr, loT, label);
    }

    sprintf(label, "$%06lX", top_addr);
    test_region((volatile long *)top_addr, loT, label);

    pline("  -- A22 boundary --");
    test_region((volatile long *)BELOW_4MB,  loT, "$3FC000");
    test_region((volatile long *)ABOVE_4MB,  loT, "$404000");

    pblank();
}

/* ---- main ---- */

int main(void)
{
    char ch;
    int  run;
    long ram_size;

    mac_init();

    gLog = fopen("DTACKResults.txt", "a");

    ram_size = physical_ram();

    if (ram_size != 0x500000L && ram_size != 0x900000L) {
        pline("=== Mac Portable DTACK Speed Test ===");
        pfmt("FAIL -- unexpected RAM size: %ld KB.", ram_size / 1024L);
        pline("Expected 5120 KB with a 4MB card");
        pline("or 9216 KB with an 8MB card.");
        pblank();
        pline("Press any key to quit.");
        wait_key();

        if (gLog)
            fclose(gLog);

        return 1;
    }

    for (run = 1; ; run++) {
        clear_window();

        pline("=== Mac Portable DTACK Speed Test ===");

        if (gLog)
            pline("Logging to DTACKResults.txt");
        else
            pline("WARNING: Results will not be saved");

        pfmt("Physical RAM = %ld KB  (card: %ld MB)",
             ram_size / 1024L,
             (ram_size - BUILTIN_SIZE) / ONE_MB);
        pblank();

        run_test(run, ram_size);

        pline("Sleep machine, wake it, press any key to run again");
        pline("-- or press Q to quit.");

        ch = wait_key();
        if (ch == 'q' || ch == 'Q')
            break;
    }

    pblank();
    pline("Done. Press any key to quit.");
    wait_key();
    if (gLog) fclose(gLog);
    return 0;
}
