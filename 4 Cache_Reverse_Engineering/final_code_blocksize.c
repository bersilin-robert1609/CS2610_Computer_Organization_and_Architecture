#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <emmintrin.h>
#include <time.h>

#define N (1024*16)
#define ass (8)

unsigned cycles_low, cycles_high, cycles_low1, cycles_high1;

static __inline__ unsigned long long rdtsc(void)
{
    __asm__ __volatile__ ("CPUID\n\t"
            "RDTSC\n\t"
            "mov %%edx, %0\n\t"
            "mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low)::
            "%rax", "rbx", "rcx", "rdx");
}

static __inline__ unsigned long long rdtsc1(void)
{
    __asm__ __volatile__ ("CPUID\n\t"
	    "RDTSC\n\t"
            "CPUID\n\t"
            "RDTSC\n\t"
            "mov %%edx, %0\n\t"
            "mov %%eax, %1\n\t": "=r" (cycles_high1), "=r" (cycles_low1)::
            "%rax", "rbx", "rcx", "rdx");
}

void swap (int *a, int *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}

void randomize ( int arr[], int n )
{
	srand ( time(NULL) );
	for (int i = n-1; i > 0; i--)
	{
		int j = rand() % (i+1);
		swap(&arr[i], &arr[j]);
	}
}

int cmpfunc (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

int main(int argc, char* argv[])
{
    uint64_t start, end;

    char arr[N];
    uint64_t times[N];
    
    //file pointer to store data in txt and in a different format in csv

    FILE *fileptr;
    FILE *fileptrcsv;

    fileptr = fopen("data1.txt", "w");
    fileptrcsv = fopen("data1.csv", "w");

    //K test cases for finding average value of block size

    int k = 101; 			//anything will work (number of testcases) (preferred odd for better median) 
    int avgmissarray[k]; 	//the miss average of each test case 
    
    fprintf(fileptrcsv,"TestCases, MeanMissRate\n");

    for (int ii = 0; ii < k; ii++) 		//for each test case
    {
        //stores the missed cache access index and the number of missed values based on a simple value classification
        int miss[N], j = 0;
        
        int avg_acc_time = 0, n = N; 	//average access time for each access and maximum possible sample space 

        //new array to randomise accessing data access
        int* it = (int*)malloc(N*sizeof(int));
	
	  //value array and randomising array declaration
        for (int i = 0; i < N; i++) {
            *(it+i) = i;
            arr[i] = '0';
        }

        //flushing the data from all levels of cache 
        for(int i = 0; i < N; i+=64) {
            _mm_clflush(arr+i);
        }

        //randomising the values of 'it' to get a permuatation from 1 to N 
        randomize(it,N);

        fprintf(fileptr, "\nN is %d: \n\n", N);
	
	   //loop to determine time for access and the determining of misses
        for(int i = 0; i < N; i++) 
        {
            int temp = it[i];
            
            //the main part
            rdtsc();
            *(arr+temp) = 'a';
            rdtsc1();

            start = ( ((uint64_t)cycles_high << 32) | cycles_low );
            end = ( ((uint64_t)cycles_high1 << 32) | cycles_low1 );
            //the main part ends

            //fprintf(fileptr, "Time %d = %lu \n", temp, end-start); (debug)

            times[temp] = end-start;

			if(times[temp] < 1e6) avg_acc_time += times[temp]; //strangely large test values
			else n--;
          
        }

        avg_acc_time /= n;

        for(int i = 0; i < N; i++){         
            if(times[i] > (1.5)*avg_acc_time) miss[j++] = i;  //THE Classification
        }

        //sorting the misses
        qsort(miss,j,sizeof(miss[0]), cmpfunc);
	
	   //finding the difference between the memory access misses and finding their average
        int avgmisssize = 0;
        for (int i = 0; i < j; i++)
        {
            fprintf(fileptr,"Miss : %d, took time %lu\n", miss[i], times[miss[i]]);
            if(i>0) avgmisssize += miss[i]-miss[i-1];
        }
        if(j>1) avgmisssize/=(j-1);
        
        fprintf(fileptr,"\nThe average miss size is %d\n", avgmisssize);
        
        avgmissarray[ii] = avgmisssize;
        
        for(int i = 0; i < N; i+=64) {
            _mm_clflush(arr+i);
        }
        
        free(it);

    }
    
    int mean = 0, median;

    for(int i=0; i<k; i++){
    	
    	mean += avgmissarray[i];
    	fprintf(fileptrcsv,"%d,%d\n",i+1,avgmissarray[i]);
    	
    }
    
    qsort(avgmissarray,k,sizeof(avgmissarray[0]),cmpfunc); //sorting the values to get the median

    median = avgmissarray[k/2];
    mean /= k;
    
    fprintf(fileptr,"\nMean is %d, Median is %d\n", mean, median);
    
    
    fclose(fileptr);

    return 0;
}
