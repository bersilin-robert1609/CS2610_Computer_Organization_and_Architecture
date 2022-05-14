%include "io.mac"          ;macro file import

.DATA                      ;initialized data

.UDATA                     ;uninitalized data (allocates memory)

.CODE                      ;the program itself
    .STARTUP
        GetLInt     ECX                       ;reading the number of numbers to be entered
        sub         EAX, EAX
        mov         EBX, ECX                  ;EAX = 0; EBX = n; ECX = n;

    Addition:
	cmp         ECX, 0		      
	jle         Done		      ;jump instruction to Done:
        GetLInt     EDX                       ;the numbers to be added in EDX
        add         EAX, EDX                  ;the sum in EAX
        sub         ECX, 1
	jmp         Addition                  ;jump instruction to Addition:
                           
    Done:	
    	PutLInt      EBX                      ;printing the number of integers to be added
    	nwln
    	PutLInt      EAX                      ;printing the sum of the entered integers
    	nwln    
    .EXIT                       