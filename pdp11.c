#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define MEM_SIZE_IN_WORDS 32*1024
#define SET 1
#define CLEAR 0
#define CLAMP_16_BIT 0177777
#define LOW_ORDER_MASK 0200000

//Condition codes
int n_psw = 0;
int z_psw = 0;
int v_psw = 0;
int c_psw = 0;

int halt = 0;

int instrExecs = 0;
int instrFetches = 0;
int memReads = 0;
int memWrites = 0;
int branches = 0;
int branch_taken = 0;

typedef struct address_phrase_t{
    int mode;
    int reg;
    int addr;
    int value;
} address_phrase_t;

int mem[MEM_SIZE_IN_WORDS]; 
int reg[8] = {0}; 
int ir; 
address_phrase_t src, dst; 

void loadMem();
void get_operand(address_phrase_t*);
void update_operand(address_phrase_t*, int);
void put_result();
void printSrcDst();
void printRegisters();

int main(int argc, char **argv) {
    bool verboseMode = false;
    bool traceMode = false;

    if(argc == 2){
    if(strcmp(argv[1], "-t") == 0){
            traceMode = true;
        }
        if(strcmp(argv[1], "-v") == 0){
            verboseMode = true;
        }
    }

    loadMem();

    int offset;
    int sign_bit;
    int result;

    if(verboseMode || traceMode) {
        printf("\ninstruction trace:\n");
    }

    //loop until halt instruction or other criteria
    while(!halt){
        /* fetch – note that reg[7] in PDP-11 is the PC */ 
        printf("at 0%04o, ", reg[7]); 
        ir = mem[ reg[7] >> 1 ];  /* adjust for word address */ 
        instrFetches++;
        assert( ir < 0200000 ); 
        reg[7] = ( reg[7] + 2 ) & CLAMP_16_BIT; 
    
        /* extract the fields for the addressing modes          */ 
        /*   (done whether src and dst are used or not; could   */ 
        /*   be moved instead to indiv. inst. execution blocks) */ 
        src.mode = (ir >> 9) & 07;  /* three one bits in the mask */ 
        src.reg = (ir >> 6) & 07;   /* decimal 7 would also work  */ 
        dst.mode = (ir >> 3) & 07; 
        dst.reg = ir & 07; 
    
        /* decode using a series of dependent if statements */ 
        /*   and execute the identified instruction         */ 
        if( ir == 0 ){  //ref 4-71
            printf("halt instruction\n"); 
            halt = 1; 
        } else if( (ir >> 12) == 01 ){   /* LSI-11 manual ref 4-25 */ 
            printf("mov instruction "); 
            if(verboseMode || traceMode){ 
                printSrcDst(); 
            }
            else { printf("\n"); }

            get_operand( &src ); 

            result = src.value;

            sign_bit = result & 0100000;

            n_psw = 0;
            if(sign_bit){ n_psw = 1;}

            z_psw = 0;
            if(result == 0){ z_psw = 1; }

            v_psw = 0;

            put_result( &dst, result);

            if(verboseMode){
                printf("  src.value = 0%06o\n", src.value);
                printf("  nzvc bits = 4'b%o%o%o%o", n_psw, z_psw, v_psw, c_psw);
            }
        } else if( (ir >> 12) == 02 ){ //ref 4-26
            printf("cmp instruction ");
            if(verboseMode || traceMode){ 
                printSrcDst(); 
            }
            else { printf("\n"); }
            
            get_operand( &src );
            get_operand( &dst );

            result = src.value - dst.value;

            c_psw = (result & 0200000) >> 16;
            result = result & CLAMP_16_BIT;

            sign_bit = result & 0100000;

            n_psw = 0;
            if( sign_bit ) { n_psw = 1; }

            z_psw = 0;
            if( result == 0 ) { z_psw = 1; }

            v_psw = 0;
            if( ((src.value & 0100000) != (dst.value & 0100000)) && ((src.value & 0100000) == (result & 0100000)) ){
                v_psw = 1;
            }

            if(verboseMode){
                printf("  src.value = 0%06o\n", src.value);
                printf("  dst.value = 0%06o\n", dst.value);
                printf("  result    = 0%06o\n", result);
                printf("  nzvc bits = 4'b%o%o%o%o\n", n_psw, z_psw, v_psw, c_psw);
            }
        } else if( (ir >> 12) == 06) { //ref 4-27
            printf("add instruction ");
            if(verboseMode || traceMode){ 
                printSrcDst(); 
            }
            else { printf("\n"); }

            get_operand( &src );
            get_operand( &dst );

            result = src.value + dst.value;

            c_psw = (result & 0200000) >> 16;
            result = result & CLAMP_16_BIT;

            sign_bit = result & 0100000;
            n_psw = 0;
            if( sign_bit ) { n_psw = 1; }

            z_psw = 0;
            if( result == 0 ) { z_psw = 1; }

            v_psw = 0;
            if( ((src.value & 0100000) != (dst.value & 0100000)) && ((src.value & 0100000) == (result & 0100000)) ){
                v_psw = 1;
            }

           if(verboseMode){
                printf("\n  src.value = 0%06o\n", src.value);
                printf("  dst.value = 0%06o\n", dst.value);
                printf("  result  = 0%06o\n", result);
                printf("  nzvc bits = 4'b%o%o%o%o\n", n_psw, z_psw, v_psw, c_psw);
            }

            update_operand(&dst, result);
        } else if( (ir >> 12) == 016) { //ref 4-28
            printf("sub instruction ");

            if(verboseMode || traceMode){ 
                printSrcDst(); 
            }
            else { printf("\n"); }

            get_operand( &src );
            get_operand( &dst );

            result = dst.value - src.value;
            //C: cleared if there was a carry from the most significant bit of
            //the result; set otherwise 
            c_psw = (result & 0200000) >> 16;
            result = result & CLAMP_16_BIT;

            sign_bit = result & 0100000;

            //N: set if result <0; cleared otherwise
            n_psw = 0;
            if( sign_bit ) { n_psw = 1; }
            //Z: set if result = 0; cleared otherwise
            z_psw = 0;
            if( result == 0 ) { z_psw = 1; }
            //V: set if there was arithmetic overflow as a result of the oper·
            //ation, that is if operands were of opposite signs and the sign
            //of the source was the same as the sign of the result; cleared
            //otherwise
            v_psw = 0;
            if( ((src.value & 0100000) != (dst.value & 0100000)) && ((src.value & 0100000) == (result & 0100000)) ){
                v_psw = 1;
            }

            if(verboseMode){
                printf("\n  src.value = 0%06o\n", src.value);
                printf("  dst.value = 0%06o\n", dst.value);
                printf("  result  = 0%06o\n", result);
                printf("  nzvc bits = 4'b%o%o%o%o\n", n_psw, z_psw, v_psw, c_psw);
            }

            update_operand(&dst, result);
        } else if( (ir >> 9) == 077) { //ref 4-61
            printf("sob instruction reg %d ", src.reg);

            offset = ir & 077; //6-bit signed offset

            printf("with offset 0%02o\n", offset);

            offset = offset << 26; //sign extend to 32 bits
            offset = offset >> 26;

            result = reg[src.reg];
            result--;

            reg[src.reg] = result;

            if(result != 0){
                reg[7] =  ( reg[7] - (offset << 1)) & 0177777;
                branch_taken++;
            }
            else{
                reg[7] = reg[7];
            }
            
            branches++;
        } else if( (ir >> 8) == 001) { //ref 4-35
            printf("br instruction ");

            offset = ir & 0377; //8-bit signed offset

            printf("with offset 0%03o\n", offset);

            offset = offset << 24; //sign extend to 32 bits
            offset = offset >> 24;

            reg[7] =  ( reg[7] + (offset << 1)) & 0177777;
            branch_taken++;

            branches++;

        } else if( (ir >> 8) == 002) { //ref 4-36
            printf("bne instruction ");

            offset = ir & 0377; //8-bit signed offset

            printf("with offset 0%03o\n", offset);

            offset = offset << 24; //sign extend to 32 bits
            offset = offset >> 24;

            if(!z_psw){
                reg[7] =  ( reg[7] + (offset << 1)) & 0177777;
                branch_taken++;
            }

            branches++;

        } else if( (ir >> 8) == 003) { //ref 4-37
            printf("beq instruction ");

            offset = ir & 0377; //8-bit signed offset

            printf("with offset 0%03o\n", offset);

            offset = offset << 24; //sign extend to 32 bits
            offset = offset >> 24;

            if(z_psw){
                reg[7] =  ( reg[7] + (offset << 1)) & 0177777;
                branch_taken++;
            }

            branches++;
        } else if( (ir >> 6) == 0062) { //ref 4-13
            //TODO: Doesn't work

            printf("asr instruction ");
            if(verboseMode || traceMode){ 
                printSrcDst(); 
            }
            else { printf("\n"); }

            get_operand( &dst );

            result = dst.value;

            result = result << 24; //sign extend
            result = result >> 24;

            c_psw = (result & LOW_ORDER_MASK) >> 16;

            result = result >> 1;
            result = result & CLAMP_16_BIT;

            n_psw = 0;
            if(result & 1){
                n_psw = SET;
            }

            z_psw = 0;
            if(result == 0){
                z_psw = 1;
            }

            v_psw = n_psw ^ c_psw;

            if(verboseMode){
                printf("  dst.value = 0%06o\n", dst.value);
                printf("  result  = 0%06o\n", result);
                printf("  nzvc bits = 4'b%o%o%o%o\n", n_psw, z_psw, v_psw, c_psw);
            }

            update_operand( &dst, result);
        } else if( (ir >> 6) == 0063) { //ref 4-14
            printf("asl instruction ");
            if(verboseMode || traceMode){ 
                printSrcDst(); 
            }
            else { printf("\n"); }

            get_operand( &dst );
            result = dst.value << 1;

            c_psw = (result & LOW_ORDER_MASK) >> 16;

            result = result & CLAMP_16_BIT;

            n_psw = 0;
            if(result & 1){
                n_psw = SET;
            }

            z_psw = 0;
            if(result == 0){
                z_psw = 1;
            }

            v_psw = n_psw ^ c_psw;

            if(verboseMode){
                printf("  dst.value = 0%06o\n", dst.value);
                printf("  result  = 0%06o\n", result);
                printf("  nzvc bits = 4'b%o%o%o%o\n", n_psw, z_psw, v_psw, c_psw);
            }

            update_operand( &dst, result);
        } else{
            printf("Error: no matching instruction" );
            instrExecs--;
        }

        instrExecs++;
        if(verboseMode){
            printRegisters();
        }
    }

    printf("\nexecution statistics (in decimal):\n");
    printf("  instructions executed     = %d\n", instrExecs);
    printf("  instruction words fetched = %d\n", instrFetches);
    printf("  data words read           = %d\n", memReads);
    printf("  data words written        = %d\n", memWrites);
    printf("  branches executed         = %d\n", branches);
    printf("  branches taken            = %d (%0.1f%%)\n", branch_taken, (double)branch_taken*100/branches);

    printf("\nfirst 20 words of memory after execution halts:\n");
    for( int i = 0; i < 20; i++){
        printf("  0%04o: 0%05o\n", 2*i, mem[i]);
    }
    return 0;
}

void loadMem() {
    printf("\nreading words in octal from stdin:\n");

    int instructionIn, count = 0;
    while( scanf("%o", &instructionIn) != EOF){
        printf("  0%06o\n", instructionIn);
        mem[count] = instructionIn;
        count++;
    }
}

void get_operand(address_phrase_t *phrase) {

    assert((phrase->mode >= 0) && (phrase->mode <= 7));
    assert((phrase->reg >= 0) && (phrase->reg <= 7));

    switch(phrase->mode) {
        /*register*/
        //The operand is in Rn
        case 0:
            phrase->value = reg[ phrase->reg ];
            assert( phrase->value < 0200000);
            phrase->addr = 0;
            break;
        //register indirect
        //Rn contains the address of the operand
        case 1:
            phrase->addr = reg[phrase->reg]; /* address is in the register*/
            assert( phrase->addr < 0200000);
            phrase->value = mem[ phrase->addr >> 1]; // adjust to word address
            assert(phrase->value < 0200000);
            break;
        //autoincrement (post reference)
        //Rn contrains the address of the operand, then increment Rn
        case 2:
            phrase->addr = reg[ phrase->reg ]; //address is in te register
            assert( phrase->addr < 0200000);
            phrase->value = mem[phrase->addr >> 1]; //adjust to word address
            instrFetches++;
            assert(phrase->value < 0200000);
            reg[ phrase->reg] = (reg[phrase->reg] + 2 ) & 0177777;
            break;
        //autoincrement indirect
        //Rn contains the address of the address of the operand, then increment Rn by 2
        case 3:
            phrase->addr = reg[phrase->reg]; //addr of addr is in reg
            assert(phrase->addr < 0200000);

            phrase->addr = mem[ phrase->addr >> 1]; //adjust to word addr
            instrFetches++;
            assert( phrase->addr < 0200000);

            phrase->value = mem[phrase->addr >> 1]; //adjust to word addr
            instrFetches++;
            assert(phrase->value < 0200000);

            reg[phrase->reg] = (reg[phrase->reg] + 2 ) & 0177777;

            break;
        //autodecrement
        //Decrement Rn, then use the result as the address of the operand
        case 4:
            reg[phrase->reg] = (reg[phrase->reg] - 2) & 0177777;
            phrase->addr = reg[phrase->reg]; // address is in the register
            assert(phrase->addr < 0200000);

            phrase->value = mem[ phrase->addr >> 1]; //adjust to word addr
            instrFetches++;
            assert(phrase->value < 0200000);
            break;
        //autodecrement indirect    
        //Decrement Rn by 2,then use the result as theaddress of the address of the operand
        case 5:
            reg[phrase->reg] = (reg[phrase->reg] - 2) & 0177777;
            phrase->addr = reg[phrase->reg]; // addr of addr is in reg
            assert(phrase->addr < 02000000);

            phrase->addr = mem[phrase->addr >> 1 ]; //adjust to word addr
            instrFetches++;
            assert(phrase->addr < 0200000);

            phrase->value = mem[ phrase->addr >> 1]; //adjust to word adr
            instrFetches++;
            assert(phrase->value < 0200000);

            break;
        //index
        //Rn+X is the address of the operand
        case 6:
            //TODO: Doesn't work

            phrase->addr = reg[phrase->reg];

            int x = mem[phrase->addr + 1];
            instrFetches++;

            int i = (phrase->addr + x) & 0177777;
            
            phrase->value = mem[i];
            instrFetches++;

            break;
        //Index deferred
        //Rn+X is the address of the address of the operand
        case 7:
            phrase->addr = reg[phrase->reg];

            int xx = mem[phrase->addr + 1];
            instrFetches++;

            int ii = (phrase->addr + xx) & 0177777;
            
            ii = mem[ii];
            instrFetches++;

            phrase->value = mem[ii];
            memReads++;

            break;
        default:
            printf("unimplemented address mode %o\n", phrase->mode);
            break;
    }
}

void update_operand(address_phrase_t *phrase, int newOp){
    if(phrase->mode == 0) {
        reg[phrase->reg] = newOp;
    }
    else {
        mem[phrase->addr] = newOp;
    }
}

void put_result(address_phrase_t *phrase, int result){
    get_operand( phrase );
    update_operand( phrase, result );
}

void printSrcDst(){
    printf("sm %d, ", src.mode);
    printf("sr %d ", src.reg);
    printf("dm %d ", dst.mode);
    printf("dr %d\n", dst.reg);
}

void printRegisters(){
    printf("  R0:0%06o  R2:0%06o  R4:0%06o  R6:0%06o\n", reg[0], reg[2], reg[4], reg[6]);
    printf("  R1:0%06o  R3:0%06o  R5:0%06o  R7:0%06o\n", reg[1], reg[3], reg[5], reg[7]);
}