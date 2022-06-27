#include <iostream>
#include <fstream>
#include <queue>
using namespace std;

int PC = 0;                                                                      // Program Counter
int ICache[256], DCache[256], RF[16];                                            // Instruction Cache, Data Cache, Register File
bool ready[16];                                                                  // Ready Bit for Registers
int num_stall = 0, num_dat_st = 0, num_con_st = 0, num_inst = 0, num_cycles = 0; // Statistics
int num_arith = 0, num_log = 0, num_con = 0, num_data = 0, num_halt = 0;         // Statistics
int halt = 0, can_fetch = 1;                                                     // Halt and Fetch Flags

void hex_to_dec(int &x)             // Converts HEX to DEC
{
    if (x & (1 << 7))
        x = x - (1 << 8);
}

void dec_to_hex(int &x)             // Converts DEC to HEX
{
    if (x < 0)
        x = x + (1 << 8);
}

class Instruction
{ // Instruction Class
private:
    int IR, args[4], NPC;             // Instruction Register, Arguments, New Program Counter
    int RA, RB, Imm, ALUOutput, Cond; // Source Registers, ALU Output, Condition
    int LMD;                          // Load Memory Data
    int is_stalled = 0;               // Stall Flag

public:
    int stage; // Stage of Instruction

    int get(int &R, int tag)
    { // Get Register Value
        if (ready[tag] == 1)            //IF register is ready
        {
            is_stalled = 0;
            R = RF[tag];
            hex_to_dec(R);
        }
        else                           //IF register is not ready
            is_stalled = 1;

        return is_stalled;
    }

    void get_args()
    { // Get Arguments
        int temp = IR;
        for (int i = 3; i >= 0; i--)
        {
            args[i] = temp & 15;
            temp = (temp >> 4);
        }
    }

    void fetch()
    { // Fetch Stage
        NPC = PC;
        IR = (ICache[NPC] << 8) + ICache[NPC + 1];
        NPC = NPC + 2;
        PC = NPC;
        stage = 1;
    }

    void decode()
    { // Decode Stage
        get_args();

        switch (args[0])
        {
        case 0: // ADD instruction
        case 1: // SUB instruction
        case 2: // MUL instruction
        case 4: // AND instruction
        case 5: // OR instruction
        case 7: // XOR instruction
            if (get(RA, args[2])) return;
            if (get(RB, args[3])) return;
            ready[args[1]] = 0;
            break;
        case 3: // INC instruction
            if (get(RA, args[1])) return;
            ready[args[1]] = 0;
            break;
        case 6: // NOT instruction
            if (get(RA, args[2])) return;
            ready[args[1]] = 0;
            break;
        case 8: // LOAD instruction
            if (get(RA, args[2])) return;
            ready[args[1]] = 0;
            Imm = args[3];
            hex_to_dec(Imm);
            break;
        case 9: // STORE instruction
            if (get(RA, args[2])) return;
            if (get(RB, args[1])) return;
            Imm = args[3];
            hex_to_dec(Imm);
            break;
        case 10: // JMP instruction
            Imm = (args[1] << 4) + args[2];
            hex_to_dec(Imm);
            can_fetch = 0;
            break;
        case 11: // BEQZ instruction
            if (get(RA, args[1])) return;
            Imm = (args[2] << 4) + args[3];
            hex_to_dec(Imm);
            can_fetch = 0;
            break;
        case 15: // HLT instruction
            halt = 1;
            break;
        }
        is_stalled = 0;
    }

    void execute()
    { // Execute Stage
        switch (args[0])
        {
        case 0: // ADD instruction
            ALUOutput = RA + RB;
            break;
        case 1: // SUB instruction
            ALUOutput = RA - RB;
            break;
        case 2: // MUL instruction
            ALUOutput = RA * RB;
            break;
        case 3: // INC instruction
            ALUOutput = RA + 1;
            break;
        case 4: // AND instruction
            ALUOutput = RA & RB;
            break;
        case 5: // OR instruction
            ALUOutput = RA | RB;
            break;
        case 6: // NOT instruction
            ALUOutput = ~RA;
            break;
        case 7: // XOR instruction
            ALUOutput = RA ^ RB;
            break;
        case 8: // LOAD instruction
            ALUOutput = RA + Imm;
            break;
        case 9: // STORE instruction
            ALUOutput = RA + Imm;
            break;
        case 10: // JMP instruction
            ALUOutput = NPC + (Imm << 1);
            PC = ALUOutput;
            break;
        case 11: // BEQZ instruction
            ALUOutput = NPC + (Imm << 1);
            Cond = (RA == 0);
            if(Cond) PC = ALUOutput;
            break;
        }
        if(args[0] < 8)
        {
            ready[args[1]] = 0;
        }
    }

    void memory_read()
    { // Memory Read Stage
        switch (args[0])
        {
        case 8: // LOAD instruction
            LMD = DCache[ALUOutput];
            ready[args[1]] = 0;
            break;
        case 9: // STORE instruction
            DCache[ALUOutput] = RB;
            break;
        case 10: // JMP instruction
            // PC = ALUOutput;
            can_fetch = 1;
            break;
        case 11: // BNZ instruction
            if (Cond)
            {
                // PC = ALUOutput;
            }
            can_fetch = 1;
            break;
        }
        if(args[0] < 8)
        {
            ready[args[1]] = 0;
        }
    }

    void register_write()
    { // Register Write Stage
        if (args[0] < 8)
        {
            dec_to_hex(ALUOutput);
            RF[args[1]] = ALUOutput;
            ready[args[1]] = 1;
        }
        else if (args[0] == 8)
        {
            dec_to_hex(LMD);
            RF[args[1]] = LMD;
            ready[args[1]] = 1;
        }
    }

    int process()
    { // Process
        switch (stage)
        {
        case 1:
            decode();
            break;
        case 2:
            execute();
            break;
        case 3:
            memory_read();
            break;
        case 4:
            register_write();
            break;
        }
        if (!is_stalled)
            next_stage();
        return is_stalled;
    }

    void next_stage()
    { // Next Stage
        stage++;
        if (stage == 5)
        {
            if (args[0] < 4)
                num_arith++;
            else if (args[0] < 8)
                num_log++;
            else if (args[0] < 10)
                num_data++;
            else if (args[0] < 12)
                num_con++;
            else
                num_halt++;
        }
    }
};

void initialize()
{ // Initialize
    for (int i = 0; i < 16; i++)
    {
        ready[i] = 1;
    }
}

void read_input(int arr[], int size, string filename)
{ // Read Input
    ifstream input;
    input.open(filename);
    string ins;
    for (int i = 0; i < size; i++)
    {
        if (!input)
            break;
        getline(input, ins);
        arr[i] = stoi(ins, 0, 16);
    }
    input.close();
}

void print_output(int arr[], int size, string filename)
{ // Print Output
    ofstream output;
    output.open(filename);
    for (int i = 0; i < size; i++)
    {
        int temp = arr[i];
        temp = temp & 0xff;
        output << hex << (temp >> 4);
        temp = temp & 0xf;
        output << hex << temp << endl;
    }
    output.close();
}

void print_stats(string filename)
{ // Print Stats
    ofstream output;
    output.open(filename);
    output << "Total number of instructions executed:" << num_inst << endl;
    output << "Number of instructions in each class" << endl;
    output << "Arithmetic instructions              :" << num_arith << endl;
    output << "Logical instructions                 :" << num_log << endl;
    output << "Data instructions                    :" << num_data << endl;
    output << "Control instructions                 :" << num_con << endl;
    output << "Halt instructions                    :" << num_halt << endl;
    output << "Cycles Per Instruction               :" << (double)num_cycles / num_inst << endl;
    output << "Total number of stalls               :" << num_stall << endl;
    output << "Data stalls (RAW)                    :" << num_dat_st << endl;
    output << "Control stalls                       :" << num_con_st << endl;
    output.close();
}

queue<Instruction> IBuf;      // Instruction Buffer

int main()
{ // Main
    read_input(ICache, 256, "./input/ICache.txt");  // Read ICache
    read_input(DCache, 256, "./input/DCache.txt");  // Read DCache
    read_input(RF, 16, "./input/RF.txt");       // Read RF

    initialize();

    ofstream();

    do
    { // Main Loop
        num_cycles++;
        int stall = 0;
        int n = IBuf.size();
        for (int i = 0; i < n; i++)
        {
            Instruction ins = IBuf.front();
            IBuf.pop();
            stall += ins.process();
            if (ins.stage < 5)
                IBuf.push(ins);
        }
        if (!can_fetch)
        {
            num_stall++;
            num_con_st++;
        }
        else if (stall)
        {
            num_stall++;
            num_dat_st++;
        }
        else if (!halt)
        {
            num_inst++;
            Instruction next_ins;
            next_ins.fetch();
            IBuf.push(next_ins);
        }
    } while (!IBuf.empty());  // End of Main Loop

    print_output(DCache, 256, "./output/ODCache.txt");  // Print DCache
    print_stats("./output/Output.txt");                 // Print Stats

    return 0;
}