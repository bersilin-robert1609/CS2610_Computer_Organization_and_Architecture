#include <iostream>
#include <fstream>
#include <vector>
#include <map>

using namespace std;

/* Register Renaming start */

int PC = 0;
int maxRRFe = 32, maxfetch = 4;
map<int, int> RRFtoARF;
int ICache[256], DCache[256], RF[16];
int num_stall = 0, num_dat_st = 0, num_con_st = 0, num_inst = 0, num_cycles = 0; // Statistics
int num_arith = 0, num_log = 0, num_con = 0, num_data = 0, num_halt = 0;         // Statistics
bool halt = 0, can_fetch = 1;                                                  // Halt and Fetch Flags

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

class ARFe
{
    public:
        ARFe()
        {
            busy = false;
        }
        int index;
        int data;
        bool busy;
        int tag;
        int destination_allocate();
        pair<bool, int> source_read();
};

class RRFe
{
    public:
        RRFe()
        {
            busy = 0;
            valid = 0;
        }
        int tag;
        int data;
        bool valid;
        bool busy;
        void update(int result);
        void regupd();
};

vector<ARFe> ARF(16);
vector<RRFe> RRF(maxRRFe);

int ARFe::destination_allocate()
{
    for(int ii = 0; ii < 32; ii++)
    {
        if(RRF[ii].busy == 0)
        {
            RRF[ii].busy = 1;
            RRF[ii].valid = 0;
            this->tag = RRF[ii].tag;
            RRFtoARF[tag] = this->index;
            this->busy = 1;
            break;
        }
    }
    return tag;
}

pair<bool, int> ARFe::source_read()
{
    pair<bool, int> answer;
    if(busy == 0)
    {
        answer.first = true;
        answer.second = data;
    }
    else {
        if(RRF[tag].valid == 1)
        {
            answer.first = true;
            answer.second = RRF[tag].data;
        }
        else
        {
            answer.first = false;
            answer.second = tag;
        }
    }
    return answer;
}

void RRFe::regupd() {
    int in = RRFtoARF[tag];
    ARF[in].data = data;
    ARF[in].busy = 0;
    busy = 0;
}

/* Register Renaming End */

class Instruction
{ // Instruction Class
public:
    int index;
    int IR, args[4], NPC;                           // Instruction Register, Arguments, New Program Counter
    int op;
    int RA, RB, Imm, ALUOutput, Cond, destTag;      // Source Registers, ALU Output, Condition
    int LMD;                                        // Load Memory Data
    bool validA, validB;
    bool finish, is_stalled = 0, decoded = 0;

    int stage; // Stage of Instruction
    
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
        index = PC/2;
        IR = (ICache[NPC] << 8) + ICache[NPC + 1];
        NPC = NPC + 2;
        PC = NPC;
        stage = 1;
    }

    void decode()
    { // Decode Stage
        get_args();
        op = args[0];
        pair<bool, int> temp;
        switch(op)
        {
            case 0: // ADD instruction
            case 1: // SUB instruction
            case 2: // MUL instruction
            case 4: // AND instruction
            case 5: // OR instruction
            case 7: // XOR instruction
                temp = ARF[args[2]].source_read();
                validA = temp.first; RA = temp.second;
                temp = ARF[args[3]].source_read();
                validB = temp.first; RB= temp.second;
                destTag = ARF[args[1]].destination_allocate();
                break;
            case 3: // INC instruction
                temp = ARF[args[2]].source_read();
                validA = temp.first; RA = temp.second;
                validB = 1; RB = 1;
                destTag = ARF[args[1]].destination_allocate();
                break;
            case 6: // NOT instruction
                temp = ARF[args[2]].source_read();
                validA = temp.first; RA = temp.second;
                validB = 1; RB = 0xff;
                destTag = ARF[args[1]].destination_allocate();
                break;
            case 8: // LOAD instruction
                temp = ARF[args[2]].source_read();
                validA = temp.first; RA = temp.second;
                validB = 1;
                Imm = args[3];
                hex_to_dec(Imm);
                destTag = ARF[args[1]].destination_allocate();
                break;
            case 9: // STORE instruction
                temp = ARF[args[2]].source_read();
                validA = temp.first; RA = temp.second;
                temp = ARF[args[1]].source_read();
                validB = temp.first; RB = temp.second;
                Imm = args[3];
                hex_to_dec(Imm);
                break;
            case 10: // JMP instruction
                Imm = (args[1] << 4) + args[2];
                validA = 1;
                validB = 1;
                hex_to_dec(Imm);
                can_fetch = 0;
                break;
            case 11: // BEQZ instruction
                temp = ARF[args[1]].source_read();
                validA = temp.first; RA = temp.second;
                Imm = (args[2] << 4) + args[3];
                validB = 1;
                hex_to_dec(Imm);
                can_fetch = 0;
                break;
            case 15: // HLT instruction
                halt = 1;
                break;
        }
        decoded = 1;
        stage++;
        //DBuf.push_back(*this);
    }
};

vector<Instruction> IBuf;
vector<Instruction> DBuf;

class ROBe
{
    public:
        Instruction I;
        bool issued, finished, busy, valid;
        int IA;
        ROBe()
        {
            issued = 0;
            finished = 0;
            busy = 0;
        }
};
vector<ROBe> ROB;

class RESe
{
    public:
        Instruction I;
        bool busy;
        bool ready;
        bool executed;
        bool finished;
        RESe()
        {
            finished = 0;
            executed = 0;
            busy = 0;
        }
};

class FunctionalUnit
{
    public:
        vector<RESe> resStation;
        FunctionalUnit()
        {
            resStation.resize(2);
        }
        void finish()
        {
            for(int i = 0; i < 2; i++) {
                if(resStation[i].executed)
                {
                    Instruction I = resStation[i].I;
                    resStation[i].busy = 0;
                    resStation[i].executed = 0;
                    if(I.op < 8) 
                        RRF[I.destTag].update(I.ALUOutput);
                    else if(I.op == 8) {
                        RRF[I.destTag].update(I.LMD);
                    }
                    resStation[i].finished = 1;
                    for(vector<ROBe>::iterator itt = ROB.begin(); itt != ROB.end(); itt++) {
                        if(itt->IA == I.index) {
                            itt->I = I;
                            itt->finished = 1;
                        }
                    }
                }
            }
        }
        void validate(int tag)
        {
            for(int i = 0; i < 2; i++) {
                if(resStation[i].busy == 0) continue;
                Instruction& I = resStation[i].I;
                if(!I.validA)
                {
                    if(I.RA == tag){
                        I.RA = RRF[I.RA].data;
                        I.validA = 1;
                    }
                }
                if(!I.validB)
                {
                    if(I.RB == tag){
                        I.RB = RRF[I.RB].data;
                        I.validB = 1;
                    }
                }
            }
        }
        int size()
        {
            int cnt = 0;
            for(int i = 0; i < 2; i++) {
                RESe R = resStation[i];
                if(R.busy) cnt++;
            }
            return cnt;
        }
};

class Arithmetic : public FunctionalUnit
{
    public:
        void process()
        {
            for(int i = 0; i < 2; i++) {
                RESe& R = resStation[i];
                if(!R.busy) continue;
                Instruction& I = R.I;
                //cout << I.RA << " " << I.RB << endl;
                if(R.busy && I.validA && I.validB)
                {
                    R.ready = 1;
                    ArithmeticFU(R);
                    for(vector<ROBe>::iterator itt = ROB.begin(); itt != ROB.end(); itt++) {
                        ROBe& RRR = *itt;
                        if(RRR.IA == I.index) {
                            RRR.issued = 1;
                        }
                    }
                }
            }
        }
        void ArithmeticFU(RESe& R) {
            Instruction& I = R.I;
            switch(I.op)
            {
                case 0:
                case 3: I.ALUOutput = I.RA + I.RB; break;
                case 1: I.ALUOutput = I.RA - I.RB; break;
                case 2: I.ALUOutput = I.RA * I.RB; break;
            }
            //cout << I.ALUOutput << endl;
            R.executed = 1;
        }
}A;

class Logical : public FunctionalUnit
{
    public:
        void process()
        {
            for(int i = 0; i < 2; i++) {
                RESe& R = resStation[i];
                if(!R.busy) continue;
                Instruction& I = R.I;
                if(R.busy && I.validA && I.validB)
                {
                    R.ready = 1;
                    LogicalFU(R);
                    for(vector<ROBe>::iterator itt = ROB.begin(); itt != ROB.end(); itt++) {
                        ROBe& RRR = *itt;
                        if(RRR.IA == I.index) {
                            RRR.issued = 1;
                        }
                    }
                }
            }
        }
        void LogicalFU(RESe& R) {
            Instruction& I = R.I;
            switch(I.op)
            {
                case 4: I.ALUOutput = I.RA & I.RB; break;
                case 5: I.ALUOutput = I.RA | I.RB; break;
                case 6: I.ALUOutput = ~ I.RA; break;
                case 7: I.ALUOutput = I.RA ^ I.RB; break;
            }
            //cout << I.ALUOutput << endl;
            R.executed = 1;
        }
}L;

class Memory : public FunctionalUnit
{
    public:
        void process()
        {
            for(int i = 0; i < 2; i++) {
                RESe& R = resStation[i];
                if(!R.busy) continue;
                Instruction& I = R.I;
                if(R.busy && I.validA && I.validB)
                {
                    R.ready = 1;
                    MemoryFU(R);
                    //R.busy = 0;
                    for(vector<ROBe>::iterator itt = ROB.begin(); itt != ROB.end(); itt++) {
                        ROBe& RRR = *itt;
                        if(RRR.IA == I.index) {
                            RRR.issued = 1;
                        }
                    }
                }
            }
        }
        void MemoryFU(RESe& R)
        {
            Instruction& I = R.I;
            switch(I.op)
            {
                case 8:
                {
                    I.ALUOutput = I.RA + I.Imm;
                    I.LMD = DCache[I.ALUOutput];
                    break; 
                }
                case 9: I.ALUOutput = I.RA + I.Imm; break;                    
            }
            //cout << I.ALUOutput << endl;
            R.executed = 1;
        }
        

}M;

class Branch : public FunctionalUnit
{
    public:
        void process()
        {
            for(int i = 0; i < 2; i++) {
                RESe& R = resStation[i];
                if(!R.busy) continue;
                Instruction& I = R.I;
                if(R.busy && I.validA && I.validB)
                {
                    R.ready = 1;
                    BranchFU(R);
                    for(vector<ROBe>::iterator itt = ROB.begin(); itt != ROB.end(); itt++) {
                        if(itt->IA == I.index) {
                            itt->issued = 1;
                        }
                    }
                }
            }
        }
        void BranchFU(RESe& R)
        {
            Instruction& I = R.I;
            switch(I.op) 
            {
                case 11: if(I.RA == 0) I.Cond = 1;
                            else I.Cond = 0;
                case 10: I.ALUOutput = I.NPC + (I.Imm << 1);
            }
            //cout << I.ALUOutput << endl;
            R.executed = 1;
        }
}B;

void RRFe::update(int result) {
    data = result;
    valid = 1;
    A.validate(tag);
    L.validate(tag);
    M.validate(tag);
    B.validate(tag);
    for(vector<Instruction>::iterator it = DBuf.begin(); it != DBuf.end(); it++) {
        Instruction& I = *it;
        if(!I.validA)
        {
            if(I.RA == tag){
                I.RA = RRF[I.RA].data;
                I.validA = 1;
            }
        }
        if(!I.validB)
        {
            if(I.RB == tag){
                I.RB = RRF[I.RB].data;
                I.validB = 1;
            }
        }
    }
    for(vector<Instruction>::iterator it = IBuf.begin(); it != IBuf.end(); it++) {
        Instruction& I = *it;
        if(!I.validA)
        {
            if(I.RA == tag){
                I.RA = RRF[I.RA].data;
                I.validA = 1;
            }
        }
        if(!I.validB)
        {
            if(I.RB == tag){
                I.RB = RRF[I.RB].data;
                I.validB = 1;
            }
        }
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
    output << "Total number of instructions executed:" << num_inst  << endl;
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

void stats(int op)
{
    num_inst++;
    if(op < 4) num_arith++;
    else if(op < 8) num_log++;
    else if(op < 10) num_data++;
    else if(op < 12) num_con++;
    else num_halt++;
}

void initialize()
{
    for(int i = 0; i < 16; i++)
    {
        ARF[i].index = i;
        ARF[i].data = RF[i];
    }
    for(int i = 0; i < 32; i++)
        RRF[i].tag = i;
}

void Complete()
{
    for(vector<ROBe>::iterator it = ROB.begin(); it != ROB.end();) {
        ROBe& Re = *it;
        Instruction& I = Re.I;
        if(Re.finished) {
            switch(I.op) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                {
                    RRF[I.destTag].regupd();
                    break;
                }
                case 9:
                {
                    DCache[I.ALUOutput] = I.RB;
                    break;
                }
                case 10:
                {
                    PC = I.ALUOutput;
                    can_fetch = 1;
                }
                case 11:
                {
                    if(I.Cond) PC = I.ALUOutput;
                    else PC = I.NPC;
                    can_fetch = 1;
                }
                case 15:
                {
                    break;
                }
            }
            stats(I.op);
            ROB.erase(it);
        }
        else {
            break;
        }
    }
}

void Finish()
{
    A.finish();
    L.finish();
    M.finish();
    B.finish();
}

void Execute()
{
    A.process();
    L.process();
    M.process();
    B.process();
}

void Dispatch()
{
    for(vector<Instruction>::iterator it = DBuf.begin(); it != DBuf.end();) {
        Instruction I = *it;
        int dispatched = 0;
        switch(I.op) {
            case 0:
            case 1:
            case 2:
            case 3:
            {
                for(vector<RESe>::iterator itt = A.resStation.begin(); itt != A.resStation.end(); itt++) {
                    if(itt->busy == 0) {
                        itt->I = I;
                        itt->busy = 1;
                        itt->ready = 0;
                        dispatched = 1;
                        break;
                    }
                }
                if(dispatched) DBuf.erase(it);
                break;
            }
            case 4:
            case 5:
            case 6:
            case 7:
            {
                for(vector<RESe>::iterator itt = L.resStation.begin(); itt != L.resStation.end(); itt++) {
                    if(itt->busy == 0) {
                        itt->I = I;
                        itt->busy = 1;
                        itt->ready = 0;
                        dispatched = 1;
                        break;
                    }
                }
                if(dispatched) DBuf.erase(it);
                break;
            }
            case 8:
            case 9:
            {
                for(vector<RESe>::iterator itt = M.resStation.begin(); itt != M.resStation.end(); itt++) {
                    if(itt->busy == 0) {
                        itt->I = I;
                        itt->busy = 1;
                        itt->ready = 0;
                        dispatched = 1;
                        break;
                    }
                }
                if(dispatched) DBuf.erase(it);
                break;
            }
            case 10:
            case 11:
            {
                for(vector<RESe>::iterator itt = B.resStation.begin(); itt != B.resStation.end(); itt++) {
                    if(itt->busy == 0) {
                        itt->I = I;
                        itt->busy = 1;
                        itt->ready = 0;
                        dispatched = 1;
                        break;
                    }
                }
                if(dispatched) DBuf.erase(it);
                break;
            }
            case 15:
            {
                dispatched = 1;
                while(it != DBuf.end()) {DBuf.erase(it);}
                break;
            }
        }
        //cout << dispatched << endl;
        if(dispatched) {
            ROBe RRR;
            RRR.I = I;
            RRR.busy = 1;
            RRR.IA = I.index;
            if(I.op == 15) RRR.finished = 1;
            ROB.push_back(RRR);
        }
        else {
            break;
            it++;
        }
    }
}

void Decode()
{
    for(vector<Instruction>::iterator it = IBuf.begin(); it != IBuf.end();)
    {
        Instruction I = *it;
        if(DBuf.size() > maxfetch) {it++;continue;}
        I.decode();
        if(I.decoded) {
            DBuf.push_back(I);
            IBuf.erase(it);
        }
        else {
            break;
        }
        if(I.op > 9) {
            while(it != IBuf.end()) {IBuf.erase(it);}
        }
    }
}

void Fetch()
{
    if(!halt && can_fetch) {
        if(IBuf.size() == maxfetch) {
            num_dat_st++;
            num_stall++;
        }
        while(IBuf.size() < maxfetch) {
            Instruction I;
            I.fetch();
            IBuf.push_back(I);
        }
    }
    else if(!can_fetch) {
        num_con_st++;
        num_stall++;
    }
}

int32_t main()
{ // Main
    read_input(ICache, 256, "./input/ICache.txt");  // Read ICache
    read_input(DCache, 256, "./input/DCache.txt");  // Read DCache
    read_input(RF, 16, "./input/RF.txt");       // Read RF

    initialize();

    do
    {
        num_cycles++;
        // Completion
        Complete();

        // Finishing
        Finish();

        // Issuing && Execution
        Execute();
        
        // Dispatch
        Dispatch();

        // Decode
        Decode();

        // Fetch
        Fetch();

    }while((!IBuf.empty() || !DBuf.empty() || !ROB.empty() || !halt));

    for(int i = 0; i < 16; i++) RF[i] = ARF[i].data;

    print_output(DCache, 256, "./output/ODCache.txt");  // Print DCache
    print_output(RF, 16, "./output/ORF.txt");  // Print RF
    print_stats("./output/Output.txt");                 // Print Stats
    
    return 0;
}