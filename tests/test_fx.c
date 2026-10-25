/* Compare all ALT/opcode combinations against the upstream interpreter.
 * This checks equivalent behavior, not independent hardware accuracy. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define HCSNES_TEST 1
#define HCSNES_THREADED_FX 1
#include "../source/fxinst.c"
#include "../source/fxemu.c"

static unsigned char rom[6*1024*1024], ram[4*65536], regs[0x300];
static unsigned char before_ram[sizeof(ram)], expected_ram[sizeof(ram)];
static unsigned char before_regs[sizeof(regs)], expected_regs[sizeof(regs)];
static unsigned rng=0x36f26001u;
static unsigned random32(void) {
   rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng;
}
static void setup(unsigned opcode, unsigned mode, unsigned seed)
{
   unsigned i;
   FxInit_s init;
   init.pvRegisters=regs; init.nRamBanks=4; init.pvRam=ram;
   init.nRomBanks=32; init.pvRom=rom;
   FxReset(&init);
   GSU.vMode=mode;
   GSU.vScreenHeight=GSU.vScreenRealHeight=(seed&1)?256:128;
   GSU.pvScreenBase=ram;
   GSU.pfPlot=fx_apfPlotTable[mode];
   GSU.pfRpix=fx_apfPlotTable[mode+5];
   fx_apfOpcodeTable[0x04c]=fx_apfOpcodeTable[0x24c]=GSU.pfPlot;
   fx_apfOpcodeTable[0x14c]=fx_apfOpcodeTable[0x34c]=GSU.pfRpix;
   fx_computeScreenPointers();
   for(i=0;i<16;++i) GSU.avReg[i]=random32()&65535;
   GSU.vStatusReg=FLG_G|(opcode&0x300)|((seed&1)?FLG_B:0);
   GSU.vPipe=opcode&255;
   GSU.vColorReg=random32()&255;
   GSU.vPlotOptionReg=seed&31;
   GSU.vZero=random32()&65535;
   GSU.vSign=random32()&65535;
   GSU.vCarry=seed&1;
   GSU.vOverflow=(int)(random32()&0x1ffff)-65536;
   GSU.pvSreg=&GSU.avReg[seed%16];
   GSU.pvDreg=&GSU.avReg[(seed*7)%16];
   /* Boundary cases exercise pipeline wrapping and ROM-buffer updates. */
   if(seed==0) {R14=R15=65535; GSU.pvDreg=&R14;}
   if(seed==1) {R15=0; GSU.pvSreg=GSU.pvDreg=&R15;}
}
static void compare(unsigned opcode, unsigned mode, unsigned seed, unsigned budget)
{
   FxRegs_s initial=GSU, expected;
   unsigned actual_count, expected_count;
   memcpy(before_ram,ram,sizeof(ram)); memcpy(before_regs,regs,sizeof(regs));
   expected_count=fx_run_reference(budget);
   expected=GSU;
   memcpy(expected_ram,ram,sizeof(ram)); memcpy(expected_regs,regs,sizeof(regs));
   GSU=initial;
   memcpy(ram,before_ram,sizeof(ram)); memcpy(regs,before_regs,sizeof(regs));
   actual_count=fx_run(budget);
   if (actual_count!=expected_count || memcmp(&GSU,&expected,sizeof(GSU)) ||
       memcmp(ram,expected_ram,sizeof(ram)) || memcmp(regs,expected_regs,sizeof(regs)))
   {
      fprintf(stderr,"FAIL opcode=%03x mode=%u seed=%u budget=%u\n",opcode,mode,seed,budget);
      exit(1);
   }
}
int main(void)
{
   unsigned i,op,mode,seed,count=0;
   for(i=0;i<sizeof(rom);++i) rom[i]=random32();
   for(i=0;i<sizeof(ram);++i) ram[i]=random32();
   for(mode=0;mode<4;++mode)
      for(op=0;op<1024;++op)
         for(seed=0;seed<8;++seed)
         {
            setup(op,mode,seed); compare(op,mode,seed,1); ++count;
         }
   for(seed=0;seed<256;++seed) {
      setup(seed,seed%4,seed);
      compare(seed,seed%4,seed,64); ++count;
      setup(seed,seed%4,seed);
      compare(seed,seed%4,seed,0); ++count;
      setup(seed,seed%4,seed); CF(G);
      compare(seed,seed%4,seed,64); ++count;
   }
   printf("PASS: %u Super FX differential cases (all 1024 entries, 4 plot modes)\n",count);
   return 0;
}
