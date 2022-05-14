%include "io.mac"

.DATA
    welcome_msg         db          "Ackermann Function Calculator A(m, n)", 0
    input_1_request     db          "Enter the value for m: ", 0
    input_2_request     db          "Enter the value for n: ", 0
    output_msg          db          "The value for the function call is: ", 0

.UDATA    

.CODE
    .STARTUP
        PutStr          welcome_msg
        nwln

        PutStr          input_1_request
        GetLInt         EBX                             ;EBX <- m

        PutStr          input_2_request
        GetLInt         ECX                             ;ECX <- n

        sub             EAX, EAX                        ;EAX <- 0

        push            EBX                             ;pushing the arguments in the stack
        push            ECX
        call            Ackermann_Func                  ;function call to Ackermann function -- A(m, n)
        pop             ECX                             ;popping the original arguments after function call
        pop             EBX                             ;can also be considered as having the respective calls arguments
        
        jmp             Done

        Ackermann_Func:
            enter           0, 0
            mov             EBX, [EBP + 12]             ;EBX <- m (local)
            mov             ECX, [EBP + 8]              ;ECX <- n (local)
            cmp             EBX, 0                      ;compare m with 0
            je              return_1                    ;jump to exit branch if equal
            cmp             ECX, 0                      ;if n == 0
            je              call_1                      ;jump to first type calling if equal
            jg              call_2                      ;jump to second type calling if greater

            call_1:
                sub             EBX, 1                  ;m <- m-1
                add             ECX, 1                  ;n <- 1 (from 0)
                push            EBX
                push            ECX
                call            Ackermann_Func          ;calling function for A(m-1, 1)
                pop             ECX
                pop             EBX
                jmp             done_func               ;return branch

            call_2:
                sub             ECX, 1                  ;n <- n-1
                push            EBX
                push            ECX
                call            Ackermann_Func          ;calling function for A(m, n-1)
                pop             ECX
                pop             EBX
                sub             EBX, 1                  ;m <- m-1
                push            EBX
                push            EAX
                call            Ackermann_Func          ;calling function for A(m-1, A(m, n-1))
                pop             ECX
                pop             EBX
                jmp             done_func               ;return branch

            return_1:
                add             ECX, 1                  ;n <- n+1
                mov             EAX, ECX                ;moving return value to EAX 
                jmp             done_func               ;return branch

            done_func:                                  
                leave                                   ;release EBP
                ret                                     ;return

        Done:
            PutStr          output_msg
            PutLInt         EAX
            nwln
            .EXIT