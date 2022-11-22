#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define MEM_SIZE_IN_WORDS 32768
#define SET 1
#define CLEAR 0
#define CLAMP_16_BIT 0177777

//Condition codes
int N;
int Z;
int V;
int C;

int halt = 0;

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

int main(int argc, char **argv) {
    bool verboseMode = false;
    bool traceMode = false;

    assert(argc > 2);
    if(strcmp(argv[1], "-t") == 0){
        traceMode = true;
    }
    if(strcmp(argv[1], "-v") == 0){
        verboseMode = true;
    }

    loadMem();

    //loop until halt instruction or other criteria
    while(!halt){
        /* fetch – note that reg[7] in PDP-11 is the PC */ 
        printf("at 0%o, ", reg[7]); 
        ir = mem[ reg[7] >> 1 ];  /* adjust for word address */ 
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
            printf("mov instruction\n"); 
            get_operand( &src ); 
        } else if( (ir >> 12) == 02 ){ //ref 4-26
            printf("cmp instruction\n");
            
        } else if( (ir >> 12) == 06) { //ref 4-27
            printf("add instruction\n");

        } else if( (ir >> 12) == 016) { //ref 4-28
            printf("sub instruction\n");
        } else if( (ir >> 9) == 077) { //ref 4-61
            printf("sob instruction\n");
        } else if( (ir >> 8) == 001) { //ref 4-35
            printf("br instruction\n");
        } else if( (ir >> 8) == 002) { //ref 4-36
            printf("bne instruction\n");
        } else if( (ir >> 8) == 003) { //ref 4-37
            printf("beq instruction\n");
        } else if( (ir >> 6) == 0062) { //ref 4-13
            printf("asr instruction\n");
        } else if( (ir >> 6) == 0063) { //ref 4-14
            printf("asl instruction\n");
        } else{
            printf("Error: no matching instruction");
        }
    }

    return 0;
}

void loadMem() {
    printf("Reading words in octal from stdin:\n");

    int instructionIn, count = 0;
    while( scanf("%o", &instructionIn) != EOF){
        printf(" 0%06o\n", instructionIn);
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
            assert(phrase->value < 0200000);
            reg[ phrase->reg] = (reg[phrase->reg] + 2 ) & 0177777;
            break;
        //autoincrement indirect
        case 3:
            phrase->addr = reg[phrase->reg]; //addr of addr is in reg
            assert(phrase->addr < 0200000);

            phrase->addr = mem[ phrase->addr >> 1]; //adjust to word addr
            assert( phrase->addr < 0200000);

            phrase->value = mem[phrase->addr >> 1]; //adjust to word addr
            assert(phrase->value < 0200000);

            reg[phrase->reg] = (reg[phrase->reg] + 2 ) & 0177777;

            break;
        //autodecrement
        case 4:
            reg[phrase->reg] = (reg[phrase->reg] - 2) & 0177777;
            phrase->addr = reg[phrase->reg]; // address is in the register
            assert(phrase->addr < 0200000);

            phrase->value = mem[ phrase->addr >> 1]; //adjust to word addr
            assert(phrase->value < 0200000);
            break;
        //autodecrement indirect    
        case 5:
            reg[phrase->reg] = (reg[phrase->reg] - 2) & 0177777;
            phrase->addr = reg[phrase->reg]; // addr of addr is in reg
            assert(phrase->addr < 02000000);

            phrase->addr = mem[phrase->addr >> 1 ]; //adjust to word addr
            assert(phrase->addr < 0200000);

            phrase->value = mem[ phrase->addr >> 1]; //adjust to word adr
            assert(phrase->value < 0200000);

            break;
        //index
        //Rn+X is he address of the operand
        case 6:
            break;
        //Index deferred
        //Rn+X is the address of the address fo the operand
        case 7:

            break;
        default:
            printf("unimplemented address mode %o\n", phrase->mode);
            break;
    }
}