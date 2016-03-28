#include <memory.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <regex.h>
#define U_L_L_I unsigned long long int
#define MAXCPUS	48

#define THRESHOLD_BETWEEN_0_6000(cond) (cond>=0 && cond <=10000)? cond: __builtin_inf()

unsigned long long int old_val_CORE[MAXCPUS], new_val_CORE[MAXCPUS];
unsigned long long int old_val_REF[MAXCPUS], new_val_REF[MAXCPUS];
unsigned long long int old_val_C3[MAXCPUS], new_val_C3[MAXCPUS];
unsigned long long int old_val_C6[MAXCPUS], new_val_C6[MAXCPUS], new_TSC[MAXCPUS], old_TSC[MAXCPUS];
int _SOCKET[MAXCPUS];
double _FREQ[MAXCPUS], _MULT[MAXCPUS];
long double C0_time[MAXCPUS], C1_time[MAXCPUS], C3_time[MAXCPUS], C6_time[MAXCPUS];

//for 64bit
static __inline__ unsigned long long
rdtsc (void)
{
    unsigned hi, lo;
__asm__ __volatile__ ("rdtsc":"=a" (lo), "=d" (hi));
    return ((unsigned long long) lo) | (((unsigned long long) hi) << 32);
}

double estimate_MHz ()
{
    //copied blantantly from http://www.cs.helsinki.fi/linux/linux-kernel/2001-37/0256.html
    /*
    * $Id: MHz.c,v 1.4 2001/05/21 18:58:01 davej Exp $
    * This file is part of x86info.
    * (C) 2001 Dave Jones.
    *
    * Licensed under the terms of the GNU GPL License version 2.
    *
    * Estimate CPU MHz routine by Andrea Arcangeli <andrea@suse.de>
    * Small changes by David Sterba <sterd9am@ss1000.ms.mff.cuni.cz>
    *
    */
    struct timezone tz;
    struct timeval tvstart, tvstop;
    unsigned long long int cycles[2];		/* must be 64 bit */
    unsigned long long int microseconds;	/* total time taken */

    memset (&tz, 0, sizeof (tz));

    /* get this function in cached memory */
    gettimeofday (&tvstart, &tz);
    cycles[0] = rdtsc ();
    gettimeofday (&tvstart, &tz);

    /* we don't trust that this is any specific length of time */
    /*1 sec will cause rdtsc to overlap multiple times perhaps. 100msecs is a good spot */
    usleep (10000);

    cycles[1] = rdtsc ();
    gettimeofday (&tvstop, &tz);
    microseconds = ((tvstop.tv_sec - tvstart.tv_sec) * 1000000) +
                   (tvstop.tv_usec - tvstart.tv_usec);

    unsigned long long int elapsed = 0;
    if (cycles[1] < cycles[0])
    {
        //printf("c0 = %llu   c1 = %llu",cycles[0],cycles[1]);
        elapsed = UINT32_MAX - cycles[0];
        elapsed = elapsed + cycles[1];
        //printf("c0 = %llu  c1 = %llu max = %llu elapsed=%llu\n",cycles[0], cycles[1], UINT32_MAX,elapsed);
    }
    else
    {
        elapsed = cycles[1] - cycles[0];
        //printf("\nc0 = %llu  c1 = %llu elapsed=%llu\n",cycles[0], cycles[1],elapsed);
    }

    double mhz = elapsed / microseconds;


    //printf("%llg MHz processor (estimate).  diff cycles=%llu  microseconds=%llu \n", mhz, elapsed, microseconds);
    //printf("%g  elapsed %llu  microseconds %llu\n",mhz, elapsed, microseconds);
    return (mhz);
}
uint64_t get_msr_value (int cpu, uint32_t reg, unsigned int highbit, unsigned int lowbit, int* error_indx)
{
    uint64_t data;
    int fd;
    //  char *pat;
    //  int width;
    char msr_file_name[64];
    int bits;
    *error_indx =0;

    sprintf (msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd = open (msr_file_name, O_RDONLY);
    if (fd < 0)
    {
        if (errno == ENXIO)
        {
            //fprintf (stderr, "rdmsr: No CPU %d\n", cpu);
            *error_indx = 1;
            return 1;
        } else if (errno == EIO) {
            //fprintf (stderr, "rdmsr: CPU %d doesn't support MSRs\n", cpu);
            *error_indx = 1;
            return 1;
        } else {
            //perror ("rdmsr:open");
            *error_indx = 1;
            return 1;
            //exit (127);
        }
    }

    if (pread (fd, &data, sizeof data, reg) != sizeof data)
    {
        perror ("rdmsr:pread");
        exit (127);
    }

    close (fd);

    bits = highbit - lowbit + 1;
    if (bits < 64)
    {
        /* Show only part of register */
        data >>= lowbit;
        data &= (1ULL << bits) - 1;
    }

    /* Make sure we get sign correct */
    if (data & (1ULL << (bits - 1)))
    {
        data &= ~(1ULL << (bits - 1));
        data = -data;
    }

    *error_indx = 0;
    return (data);
}

uint64_t set_msr_value (int cpu, uint32_t reg, uint64_t data)
{
    int fd;
    char msr_file_name[64];

    sprintf (msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd = open (msr_file_name, O_WRONLY);
    if (fd < 0)
    {
        if (errno == ENXIO)
        {
            fprintf (stderr, "wrmsr: No CPU %d\n", cpu);
            exit (2);
        } else if (errno == EIO) {
            fprintf (stderr, "wrmsr: CPU %d doesn't support MSRs\n", cpu);
            exit (3);
        } else {
            perror ("wrmsr:open");
            exit (127);
        }
    }

    if (pwrite (fd, &data, sizeof data, reg) != sizeof data)
    {
        perror ("wrmsr:pwrite");
        exit (127);
    }
    close(fd);
    return(1);
}

int setActualCpuClockRate()
{
    int i, ii, CPU_NUM, error_indx;
    double estimated_mhz = estimate_MHz ();
    unsigned long long int CPU_CLK_UNHALTED_CORE, CPU_CLK_UNHALTED_REF, CPU_CLK_C3, CPU_CLK_C6, CPU_CLK_C1, CPU_CLK_C7;
    int numCPUs = 4;

    //some init registers before we can use performance counters
    int IA32_PERF_GLOBAL_CTRL = 911; //38F
    int IA32_PERF_GLOBAL_CTRL_Value;
    IA32_PERF_GLOBAL_CTRL_Value = get_msr_value (CPU_NUM, IA32_PERF_GLOBAL_CTRL, 63, 0, &error_indx);

    int IA32_FIXED_CTR_CTL = 909; //38D
    int IA32_FIXED_CTR_CTL_Value;
    IA32_FIXED_CTR_CTL_Value = get_msr_value (CPU_NUM, IA32_FIXED_CTR_CTL, 63, 0, &error_indx);

    int PLATFORM_INFO_MSR = 206;	//CE 15:8
    int PLATFORM_INFO_MSR_low = 8;
    int PLATFORM_INFO_MSR_high = 15;
    int CPU_Multiplier = get_msr_value (0, PLATFORM_INFO_MSR, PLATFORM_INFO_MSR_high, PLATFORM_INFO_MSR_low, &error_indx);

    //Blck is basically the true speed divided by the multiplier
    float BLCK = estimated_mhz / (float) CPU_Multiplier;
    //printf("estimated_mhz %f, BLCK %f, MULT %d\n", estimated_mhz, BLCK, CPU_Multiplier);
    struct timespec sleeptimer={0};
    sleeptimer.tv_nsec = 399999999; // 100msec, one more 9 and it will make it to 1s

    for(i=0; i< MAXCPUS; i++) {
        old_val_CORE[i] = new_val_CORE[i] = old_val_REF[i] = new_val_REF[i] = old_val_C3 [i] = new_val_C3 [i] = 0;
        old_val_C6[i] = new_val_C6[i] = new_TSC[i] = old_TSC[i] = 0;
        _FREQ[i] = _MULT[i] = 0;
        C0_time[i] = C1_time[i] = C3_time[i] = C6_time[i] = 0;
    }

    for (ii = 0; ii <  MAXCPUS; ii++) {
        //read from the performance counters
        //things like halted unhalted core cycles

        CPU_NUM = ii;
        old_val_CORE[ii] = get_msr_value (CPU_NUM, 778, 63, 0, &error_indx);
        old_val_REF[ii] = get_msr_value (CPU_NUM, 779, 63, 0, &error_indx);
        old_val_C3[ii] = get_msr_value (CPU_NUM, 1020, 63, 0, &error_indx);
        old_val_C6[ii] = get_msr_value (CPU_NUM, 1021, 63, 0, &error_indx);
        old_TSC[ii] = rdtsc ();
    }

    //sleep and let the counters increment
    nanosleep (&sleeptimer, NULL);

    //printf("Id Freq Mult\n");
    for (ii = 0; ii <  MAXCPUS; ii++) {
        //note down the counters after the sleep
        CPU_NUM = ii;
        new_val_CORE[ii] = get_msr_value (CPU_NUM, 778, 63, 0, &error_indx);
        new_val_REF[ii] = get_msr_value (CPU_NUM, 779, 63, 0, &error_indx);
        new_val_C3[ii] = get_msr_value (CPU_NUM, 1020, 63, 0, &error_indx);
        new_val_C6[ii] = get_msr_value (CPU_NUM, 1021, 63, 0, &error_indx);
        new_TSC[ii] = rdtsc ();

        if (old_val_CORE[ii] > new_val_CORE[ii]) {			//handle overflow
            CPU_CLK_UNHALTED_CORE = (UINT64_MAX - old_val_CORE[ii]) + new_val_CORE[ii];
        } else {
            CPU_CLK_UNHALTED_CORE = new_val_CORE[ii] - old_val_CORE[ii];
        }

        //number of TSC cycles while its in halted state
        if ((new_TSC[ii] - old_TSC[ii]) < CPU_CLK_UNHALTED_CORE) {
            CPU_CLK_C1 = 0;
        } else {
            CPU_CLK_C1 = ((new_TSC[ii] - old_TSC[ii]) - CPU_CLK_UNHALTED_CORE);
        }

        if (old_val_REF[ii] > new_val_REF[ii]) {			//handle overflow
            CPU_CLK_UNHALTED_REF = (UINT64_MAX - old_val_REF[ii]) + new_val_REF[ii];	//3.40282366921e38
        } else {
            CPU_CLK_UNHALTED_REF = new_val_REF[ii] - old_val_REF[ii];
        }

        if (old_val_C3[ii] > new_val_C3[ii]) {			//handle overflow
            CPU_CLK_C3 = (UINT64_MAX - old_val_C3[ii]) + new_val_C3[ii];
        } else {
            CPU_CLK_C3 = new_val_C3[ii] - old_val_C3[ii];
        }

        if (old_val_C6[ii] > new_val_C6[ii]) {			//handle overflow
            CPU_CLK_C6 = (UINT64_MAX - old_val_C6[ii]) + new_val_C6[ii];
        } else {
            CPU_CLK_C6 = new_val_C6[ii] - old_val_C6[ii];
        }

        _FREQ[ii] = THRESHOLD_BETWEEN_0_6000(estimated_mhz * ((long double) CPU_CLK_UNHALTED_CORE / (long double) CPU_CLK_UNHALTED_REF));
        _MULT[ii] = _FREQ[ii] / BLCK;

        C0_time[ii] = ((long double) CPU_CLK_UNHALTED_REF / (long double) (new_TSC[ii] - old_TSC[ii]));
        C1_time[ii] = ((long double) CPU_CLK_C1 / (long double) (new_TSC[ii] - old_TSC[ii]));
        C3_time[ii] = ((long double) CPU_CLK_C3 / (long double) (new_TSC[ii] - old_TSC[ii]));
        C6_time[ii] = ((long double) CPU_CLK_C6 / (long double) (new_TSC[ii] - old_TSC[ii]));

        if (C0_time[ii] < 1e-2) {
            if (C0_time[ii] > 1e-4) {
                C0_time[ii] = 0.01;
            } else {
                C0_time[ii] = 0;
            }
        }
        if (C1_time[ii] < 1e-2) {
            if (C1_time[ii] > 1e-4) {
                C1_time[ii] = 0.01;
            } else {
                C1_time[ii] = 0;
            }
        }
        if (C3_time[ii] < 1e-2) {
            if (C3_time[ii] > 1e-4) {
                C3_time[ii] = 0.01;
            } else {
                C3_time[ii] = 0;
            }
        }
        if (C6_time[ii] < 1e-2) {
            if (C6_time[ii] > 1e-4) {
                C6_time[ii] = 0.01;
            } else {
                C6_time[ii] = 0;
            }
        }
        //C1 time as saved in register is actually C1+C3+C6+C7 (all sleep states), so the real C1 time is basically a minus
        long double actual_c1_time = C1_time[ii] - (C3_time[ii] + C6_time[ii]);
    }
    return 1;
}

int setCpuSocketInfo()
{
    regex_t re_physicalid, re_socket0, re_socket1;
    int ii, reti, socket;
    char msgbuf[100];

    regcomp(&re_physicalid, "^physical[ \t]*id[ \t]*:[ \t]*[0-9]", 0);
    regcomp(&re_socket0, "^physical[ \t]*id[ \t]*:[ \t]*0", 0);
    regcomp(&re_socket1, "^physical[ \t]*id[ \t]*:[ \t]*1", 0);

    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen("/proc/cpuinfo", "r");
    if (fp == NULL) {
        printf("Cannot open /proc/cpuinfo");
        exit(EXIT_FAILURE);
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        reti = regexec(&re_physicalid, line, 0, NULL, 0);
        if (!reti) {
            reti = regexec(&re_socket0, line, 0, NULL, 0);
            if (!reti) {
                socket = 0;
            }
            reti = regexec(&re_socket1, line, 0, NULL, 0);
            if (!reti) {
                socket = 1;
            }
            _SOCKET[ii] = socket;
            ii++;
        }
    }

    fclose(fp);
    if (line)
        free(line);
}

void print_usage() {
    printf("Usage: -d for debug\n");
}

int main(int argc, char *argv[])
{
    setCpuSocketInfo();
    setActualCpuClockRate();

    int option = 0;
    int debug = 0, area = -1;

    while ((option = getopt(argc, argv,"apd")) != -1) {
        switch (option) {
             case 'd' : debug = 1;
                 break;
             case 'a' : area = 0;
                 break;
             default: print_usage(); 
                 exit(EXIT_FAILURE);
        }
    }

    if(debug == 1)
        printf("Set debug on\n");

    int ii, socket0_cpunum, socket1_cpunum;
    long socket0, socket1;
    socket0_cpunum = socket1_cpunum = 0;
    socket0 = socket1 = 0;

    if(debug == 1)
        printf("ProcId\tSocket\tFreq\t\tMult\n");
    for (ii = 0; ii < MAXCPUS; ii++) {
        if(_FREQ[ii] != INFINITY) {
            if(debug == 1)
                printf("%d\t%d\t%f\t%f\n", ii, _SOCKET[ii], _FREQ[ii], _MULT[ii]);
            if(_SOCKET[ii] == 0) {
                socket0_cpunum += 1;
                socket0 += (long)floor(_FREQ[ii]);
            }
            if(_SOCKET[ii] == 1) {
                socket1_cpunum += 1;
                socket1 += (long)floor(_FREQ[ii]);
            }
        }
    }
    if(socket0_cpunum > 0)  {
        printf( "%lu", (socket0 / socket0_cpunum));
    }
    if(socket1_cpunum > 0)  {
        printf( " %lu", (socket1 / socket1_cpunum));
    }
    printf("\n");
}
