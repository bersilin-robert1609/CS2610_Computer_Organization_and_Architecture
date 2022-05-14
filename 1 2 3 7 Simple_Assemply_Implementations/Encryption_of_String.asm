%include "io.mac"

.DATA 
    key             db      "5791368024", 0                                                 ;The encryption key 
    input_query     db      "Do you want to terminate the program? (y/n): ", 0              ;The query line
    input_request   db      "Input a string to encrypt: ", 0                                ;The input entry
    invalid_error   db      "Invalid Input.", 0                                             ;Invalid query error
    output_msg      db      "The encrypted string is: ", 0                                  ;The output message

.UDATA
    string          resb    21              ;Reserved space for input string

.CODE
    .STARTUP
        get_input:                          ;The function for getting input
            PutStr      input_request
            GetStr      string, 21
            mov         EBX, key
            mov         EDX, string
            PutStr      output_msg

        loop_begin:                         ;The encryption function which changes only digits
            mov         AL, [EDX]
            cmp         AL, 0
            je          ask_again
            sub         AL, 48                
            cmp         AL, 0               ;Verifying digit status
            jl          Skip
            cmp         AL, 9               ;Verifying digit status
            jg          Skip
            xlatb                           ;The special operator 'xlat'
            PutCh       AL
            add         EDX, 1
            jmp         loop_begin
        
        Skip:                               ;The skipping function for non-digit characters
            PutCh       [EDX]
            add         EDX, 1
            jmp         loop_begin

        ask_again:                          ;The query function
            nwln
            PutStr      input_query
            GetCh       AL
            cmp         AL, 'y'
            je          Done
            cmp         AL, 'Y'
            je          Done
            cmp         AL, 'n'
            je          get_input
            cmp         AL, 'N'
            je          get_input
            jmp         invalid_input

        invalid_input:                      ;The invalid input function
            PutStr      invalid_error
            jmp         ask_again

        Done:                               ;The exit_code function

    .EXIT

    