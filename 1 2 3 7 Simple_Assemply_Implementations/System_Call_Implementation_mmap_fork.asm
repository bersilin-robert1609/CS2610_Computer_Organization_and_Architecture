%include "io.mac"
%define BUFFER_SIZE 4096

.DATA
    file_open_error     db    "File Open Error", 0
    input_file_name     db    "input.txt", 0
    success_msg         db    "Success", 0
    parent_msg          db    "Parent Process:", 0
    child_msg           db    "Child Process:", 0
    input_prompt        db    "Enter the number of integers: ",0
    input2_prompt       db    "Enter the integers:",0

.UDATA
    file_desc           resd    1                       ;the file descriptor for the created file
    num_count           resd    1                       ;the number of integers in the array
    byte_count          resd    1                       ;the number of bytes sent into the file (this includes newline characters)
    file_array          resb    1                       ;the file array address which gets mapped by mmap2 syscall
    file_buf            resb    BUFFER_SIZE             ;the file buffer used to store the integer array given as input to be sent into file
    integer_array       resd    BUFFER_SIZE             ;the integer array made after parsing string to integers
    num                 resb    11                      ;every number entered taken as string

.CODE
    .STARTUP
        mov         EAX, 8                              ;the syscall code for creating file SYS_CREATE
        mov         EBX, input_file_name                ;the file name given as parameter
        mov         ECX, 777o                           ;the fie permissions
        int         0x80                                ;software interrupt      
        mov         [file_desc], EAX                    ;file descriptor returned by syscall
        cmp         EAX, 0                              
        jge         write_into_file                     ;if Successful
        PutStr      file_open_error                     ;else
        jmp         done

    write_into_file:
        PutStr      input_prompt
        GetLInt     ECX                                 ;the number of integers
        mov         [num_count], ECX                    
        mov         ECX, 0
        mov         [byte_count], ECX
        
        PutStr      input2_prompt
        nwln
        mov         EBX, file_buf
        mov         ECX, [num_count]  

        loop_begin:                                     ;reading integers from stdin (file-descriptor = 0)
            cmp         ECX, 0
            jle         loop_done
            GetStr      num, 11
            mov         EDX,num

            itos_loop:                                  ;the integer to string parse loop
                mov         AL, [EDX]
                cmp         AL, 0
                je          loop_end
                mov         [EBX], AL
                inc         EBX
                mov         EAX, 1
                add         [byte_count], EAX
                inc         EDX
                jmp         itos_loop

            loop_end:                                   ;adding a newline character after each number
                mov         EDX, 10
                mov         [EBX], EDX
                inc         EBX
                mov         EAX, 1
                add         [byte_count], EAX
                dec         ECX
                jmp         loop_begin

        loop_done:                                      ;after reading all integers and converting them into a string pointedd by file_buf
            mov         EAX, 4                          ;the syscall code for SYS_WRITE
            mov         EBX, [file_desc]                ;the file descriptor passed as parameter
            mov         ECX, file_buf                   
            mov         EDX, [byte_count]
            int         0x80

            mov         [byte_count], EAX
            
            mov         EAX, 6                          ;syscall for SYS_CLOSE
            mov         EBX, [file_desc]                ;file descriptor
            int         0x80

            jmp         memory_map
        
    memory_map:

        ;open the input.txt file

        mov         EAX, 5                              ;syscall for SYS_OPEN
        mov         EBX, input_file_name                ;file name as parameter
        mov         ECX, 2                              ;file access mode ("2" for read/write)
        mov         EDX, 777o                           ;file permissions
        int         0x80

        mov         [file_desc], EAX

        mov           EAX, 192		                    ;mmap2 instruction code
        mov           EBX, 0		                    ;address
        mov           EDX, 0x7		                    ;protection
        mov           ESI, 0x1	                        ;flags
        mov           EDI, [file_desc] 	                ;file descriptor
        mov           ECX, 4096                         ;size
        mov           EBP, 0		                    ;offset is 0
        int           0x80			                    ;For mmap

        mov           EBX, EAX
        mov           [file_array], EBX                 

        mov           EAX, 6                            ;syscall for SYS_CLOSE
        mov           EBX, [file_desc]                  ;file descriptor
        int           0x80

        mov           ECX, [byte_count]
        mov           EDX, 0                    ;stores integer from file
        mov           EAX, 0                    
        mov           ESI, 0                    ;index of integer_array

        stoi_loop_begin:
            cmp         ECX, 0
            je          stoi_loop_exit

            get_int:                            ;to get integer from file which is in string format
                mov         AL, [EBX]
                inc         EBX
                dec         ECX
                cmp         AL, 10              ;"\n" in file
                je          stoi_loop_end
                and         AL, 0Fh
                imul        EDX, 10
                add         EDX, EAX
                jmp         get_int

            stoi_loop_end:
                mov         [integer_array + 4*ESI], EDX    ;adds integer to file
                inc         ESI
                mov         EDX, 0
                jmp         stoi_loop_begin

        stoi_loop_exit:
            mov         EAX, 2                          ;the syscall code for SYS_FORK                           
            int         0x80                            ;software interrupt
            cmp         EAX, 0                          ;returns 0 if possible to create a new process
            jz          child                           ;breaks into child process
    
    parent:                                             ;sort by insertion
        PutStr      parent_msg                          ;the parent file message
        nwln
        call        insertion_sort                      ;calling the insertion_sort function
        call        print                               ;calling the print function
        jmp         done

    insertion_sort:                                     ;the function to sort an integer_array in memory
        enter       0, 0

        mov         EAX, 0                              ;the value of i in the loop
        mov         ECX, [num_count]                    ;the value of n - number of integers

        insert:                                         ;external loop for i
            mov         EBX, [integer_array + 4*EAX]    ;the value at i-th index stored in EBX
            mov         EDX, EAX                        ;the value of j

            while_loop:                                 ;internal loop for j
                cmp         EDX, 0                      ;until j reaches 0 from i
                je          end_while                   ;internal loop ends

                cmp         [integer_array + 4*(EDX-1)], EBX            ;comparing Array[i] and Array[j-1]
                jle         end_while                                   ;our loop ends of we get a less value at Array[j-1]

                mov         ESI, [integer_array + 4*(EDX-1)]            ;swapping if found a proper place to insert
                mov         [integer_array + 4*(EDX-1)], EBX
                mov         [integer_array + 4*EDX], ESI

                dec         EDX                                         ;j <- j-1
                jmp         while_loop                                  

            end_while:
                add         EAX, 1                                      ;i <- i+1
                loop        insert

        leave
        ret

    print:                                              ;function to print the integers from the integer
        enter       0, 0
        mov         EAX, 0
        mov         EBX, integer_array                                  
        mov         ECX, [num_count]

        repeat:
            PutLInt     [EBX + 4*EAX]                   ;printing in count of 4 bytes each
            nwln
            inc         EAX
            loop        repeat
        
        leave
        ret

    child:                                              ;compute sum
        
        mov         ECX, 100000                         ;just to delay child process output
        delay:
        loop        delay

        PutStr      child_msg
        nwln
        mov         EAX, 0
        mov         EBX, integer_array
        mov         ECX, [num_count]
        mov         EDX, 0                              ;stores sum

    sum:
        add         EDX, [EBX + 4*EAX]                  ;adding index EAX of integer_array  to EDX
        inc         EAX                                 ;increment EAX
        loop        sum
        
        PutLInt     EDX
        nwln
        jmp         done

    done:
    .EXIT