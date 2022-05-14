#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <emmintrin.h>
#include <time.h>

#define N (1024*256)
#define ass (8)

unsigned cycles_low, cycles_high, cycles_low1, cycles_high1;

static __inline__ unsigned long long rdtsc(void)
{
    __asm__ __volatile__ ("CPUID\n\t"
            "RDTSCP\n\t"
            "mov %%edx, %0\n\t"
            "mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low)::
            "%rax", "rbx", "rcx", "rdx");
}

static __inline__ unsigned long long rdtsc1(void)
{
    __asm__ __volatile__ ("CPUID\n\t"
            "RDTSCP\n\t"
            "CPUID\n\t"
            "RDTSCP\n\t"
            "mov %%edx, %0\n\t"
            "mov %%eax, %1\n\t": "=r" (cycles_high1), "=r" (cycles_low1)::
            "%rax", "rbx", "rcx", "rdx");
}

int main(int argc, char* argv[])
{
    uint64_t start, end;

    char arr[N];

    int k = 10;

    int m = 15;
    printf("\nThe execution times for the value of k as %d (given as m here)\n", m);
    
    for (int ii = 0; ii < k; ii++)
    {
	
	printf("\n\nTest case %d \n", ii);
	
        for (int i = 0; i < m; i++) {
            arr[i*64*256] = '0';
        }

        for(int i = 0; i < m; i+=64*256) {
            _mm_clflush(arr+i);
        }
        
        for(int i = 0; i < m; i++) 
        {
            int temp = i*64*256;
            rdtsc();
            *(arr+temp) = ' ';
            rdtsc1();

            start = ( ((uint64_t)cycles_high << 32) | cycles_low );
            end = ( ((uint64_t)cycles_high1 << 32) | cycles_low1 );

            printf("Time %d = %lu \n", temp, end-start);

        }
        
        printf("Phase 1 done\n");

        for(int i = 0; i < m; i++) 
        {
            int temp = i*64*256;
            rdtsc();
            *(arr+temp) = ' ';
            rdtsc1();

            start = ( ((uint64_t)cycles_high << 32) | cycles_low );
            end = ( ((uint64_t)cycles_high1 << 32) | cycles_low1 );

            printf("Time %d = %lu \n", temp, end-start);

        }
        
        for(int i = 0; i < m; i+=64*256) {
            _mm_clflush(arr+i);
        }

    }

    return 0;
}
