/*
 * Sim68k.c
 *   Ported from Sim68k.pas - a Pascal program originally created in Nov 1999 for CSI2111
 *                          - simulates the actions of the Motorola 68000 microprocessor
 *   Author: Mark Sattolo <epistemik@gmail.com>
 * ------------------------------------------------------------------------------------------------------
 *   $File: //depot/Eclipse/CPP/Workspace/Sim68k/src/Sim68k.c $
 *   $Revision: #16 $
 *   $Change: 107 $
 *   $DateTime: 2011/02/13 18:23:12 $   
 */

#include "SimUnit.h" // Library containing useful functions

/*
 *  VARIABLES
 *=========================================================================================================*/

byte memory[memorySize] ; // store the binary program

// The CPU registers
int TMPS, TMPD, TMPR ;  // Temporary Registers
word OpCode ;           // OPCODE of the current instruction
word OpAddr1, OpAddr2 ; // Operand Addresses

int  D[2] ; // Data Registers
word A[2] ; // Address Registers

word MAR ;  // Memory Address Register
int  MDR ;  // Memory Data Register

// store data from opCode for Format F2
byte opcData ;

// temp storage for address Mode (from opCode) for operands 1 & 2
enum addressmode M1, M2 ;

// temp storage for Register # (from opCode) for operands 1 & 2
byte R1, R2 ;

// Most Significant Bits of TMPS, TMPD, & TMPR
boolean Sm, Dm, Rm ;

/*
 *   FUNCTIONS
 *=========================================================================================================*/

// Read into memory a machine language program contained in a file 
boolean Loader( string name )
{
  boolean inComment = False ;
  word address = 0 ;
  char ch ;
  FILE *f68b ; // Your program in machine language
  
  if( (f68b = fopen(name, "r")) == NULL )
  {
    printf( "Can't open file '%s' !\n\tEXITING LOADER.\n", name );
    return False ;
  }
  
  // read characters from the file
  while( (ch = getc(f68b) ) != EOF )
  {
    // beginning & end of comment sections
    if( ch == COMMENT_MARKER )
    {
      inComment = ! inComment ;
      continue ;
    }
    
    // skip comment
    if( inComment )
      continue ;
    
    // skip char and end comment if EOL
    if( ch == EOL )
    {
      inComment = False ;
      continue ;
    }
    
    // have hex input
    if( ch == HEX_MARKER )
    {
      fscanf( f68b, "%x", memory+address );
#if DEBUG > 1
      printf( "Read value %x into memory\n", memory[address] );
#endif
      address++ ;
    }
    
  }
  
  printf( "Program loaded! Have %d bytes in memory.\n\n", address );
  
  if( fclose(f68b) != 0 )
    printf( "*** ERROR in closing file '%s'!\n", name );
  
  return True ;
  
}// Loader()

// Copies an element (Byte, Word, Long) from memory\CPU to CPU\memory.
// Verifies if we are trying to access an address outside the range allowed for addressing [0x0000..0x1000].
// Uses the RW (read|write) bit.
// Parameter dsz determines the data size (byte, word, int/long).
void accessMemory( enum dataSize dsz )
{
  //MAR = 0x1001 ;
  if( (MAR >= 0) && (MAR < memorySize) ) // Valid Memory Address
  {
    if( RW == Read ) // Read = copy an element from memory to CPU
    { 
      switch( dsz )
      {
        case byteSize: MDR = memory[MAR] ; break;
        
        case wordSize: MDR = memory[MAR] * 0x100 + memory[MAR+1] ; break;
        
        case  intSize: MDR = ( (memory[MAR]   * 0x1000000) & 0xFF000000 ) |
                             ( (memory[MAR+1] * 0x10000)   & 0x00FF0000 ) |
                             ( (memory[MAR+2] * 0x100)     & 0x0000FF00 ) |
                             (  memory[MAR+3]              & 0x000000FF ); break;
        
        default: printf( "*** ERROR >> accessMemory() received invalid data size '%d' at PC = %d\n", dsz, (PC-2) );
        H = True ;
      }
#if DEBUG > 0
      printf( "accessMemory(%s) READ: MDR now has value = %#X \n", sizeName[dsz], MDR );
#endif
    }
    else
    {
      if( RW == Write ) // Write = copy an element from the CPU to memory
      {
        switch( dsz )
        {
          case byteSize: memory[MAR] = MDR % 0x100 ; // LSB: 8 last bits
#if DEBUG > 0
                         printf( "accessMemory(%s) WRITE: memory[%#X] now has value = %#X \n",
                                 sizeName[dsz], MAR, memory[MAR] );
#endif
                         break;
                         
          case wordSize: memory[MAR]   = (MDR / 0x100) % 0x100 ; // MSB: 8 first bits
                         memory[MAR+1] =  MDR % 0x100 ; // LSB: 8 last bits
#if DEBUG > 0
                         printf( "accessMemory(%s) WRITE: memory[%#X] now has value = %#X%X \n",
                                 sizeName[dsz], MAR, memory[MAR], memory[MAR+1] );
#endif
                         break;
          case intSize:
                        memory[MAR]   = (MDR >> 24) & 0x000000FF ; // MSB: 8 first bits 
                        memory[MAR+1] = (MDR >> 16) & 0x000000FF ;
                        memory[MAR+2] = (MDR >> 8 ) & 0x000000FF ;
                        memory[MAR+3] =  MDR % 0x100 ; break;
#if DEBUG > 0
                        printf( "accessMemory(%s) WRITE: memory[%#X] now has value = %#X%X%X%X \n",
                                sizeName[dsz], MAR, memory[MAR], memory[MAR+1], memory[MAR+2], memory[MAR+3] );
#endif
                        break;
                        
          default: printf( "*** ERROR >> accessMemory() received invalid data size '%d' at PC = %d\n", dsz, (PC-2) );
          H = True ;
        }
      }// write
      else
        {
          printf( "*** ERROR >> accessMemory() received invalid RW bit '%d' \n", RW );
          H = True ;
          return ;
        }
    }
  }
  else // Invalid Memory Address
    {
      printf( "*** ERROR >> accessMemory() uses the invalid address %#X at PC = %d\n", MAR, (PC-2) );
      H = True ; // End of simulation...! 
    }
}// accessMemory()

// Fetch the OpCode from memory 
void FetchOpCode()
{
#if DEBUG > 0
  printf( "\nFetchOpCode(): at PC = %d \n", PC );
#endif
  
  RW = Read ;
  MAR = PC ;
  PC = PC + 2 ;
  accessMemory( wordSize );
  OpCode = GetWord( MDR, Least ); // get LSW from MDR
  
}// FetchOpCode()

// Update the fields OpId, DS, numOprd, M1, R1, M2, R2 and Data according to given format.
// Uses GetBits() 
void DecodeInstr()
{
    DS    = GetBits( OpCode,  9, 10 );
   OpId   = GetBits( OpCode, 11, 15 );
  numOprd = GetBits( OpCode,  8,  8 ) + 1 ;
  
#if DEBUG > 0
  printf( "DecodeInstr(OpCode %#X): at PC = %d : OpId = %d, size = %s, numOprnd = %d\n",
           OpCode, (PC-2), OpId, sizeName[DS], numOprd );
#endif
  
  if( (numOprd == noOne) || (numOprd == noTwo) ) // SHOULD ALWAYS BE TRUE!
  {
    M2 = GetBits( OpCode, 1, 3 );
    R2 = GetBits( OpCode, 0, 0 );
    
    if( FormatF1(OpId) )
    {
      if( (OpId < iDSR) )
      {
        M1 = GetBits( OpCode, 5, 7 );
        R1 = GetBits( OpCode, 4, 4 );
      }
      else /* NEED to reset these for iDSR and iHLT ! */
          M1 = R1 = M2 = R2 = 0 ;
    }
    else // Format F2
        opcData = GetBits( OpCode, 4, 7 );
  }
  else
    {
      printf( "*** ERROR >> DecodeInstr() received invalid value for number of operands '%d' at PC = %d\n",
              numOprd, (PC-2) );
      H = True ;
    }  
}// DecodeInstr()

// Fetch the operands, according to their number (numOprd) & addressing modes (M1 or M2)
void FetchOperands()
{
#if DEBUG > 0
  printf( "FetchOperands(%d): at PC = %d : M1 = %d, M2 = %d\n", numOprd, (PC-2), M1, M2 );
#endif
  
  RW = Read ;
  
  // Fetch the address of first operand (in OpAddr1)
  if( FormatF1(OpId) && (M1 == RELATIVE_ABSOLUTE) )
  {
    MAR = PC ;
    accessMemory( wordSize );
    OpAddr1 = GetWord( MDR, Least ); // get LSW of MDR
    PC = PC + 2 ;
  }
  
  // Fetch the address of 2nd operand, if F1 & 2 operands.
  // OR, operand of an instruction with format F2 put in OpAddr2
  if( M2 == RELATIVE_ABSOLUTE )
  {
    MAR = PC ;
    accessMemory( wordSize );
    OpAddr2 = GetWord( MDR, Least ); // get LSW of MDR
    PC = PC + 2 ;
  }    
  
  // Check invalid number of operands. 
  if( (numOprd == noTwo) && (!FormatF1(OpId)) )
  {
    printf( "*** ERROR >> FetchOperands() has an Invalid number of operands for %s at PC = %d\n",
            Mnemo[OpId], (PC-2) );
    H = True ;
  }

}// FetchOperands()

/****************************************************************************
  Since many instructions will make local fetches between temporary registers
  (TMPS, TMPD, TMPR) & memory or the Dn & An registers it would be    
  useful to create procedures to transfer the words/bytes between them.      
  Here are 2 suggestions of procedures to do this.                           
*****************************************************************************/

// Transfer data in the required temporary register 
void FillTmpReg( int* tmpReg,            // tmp Register to modify - TMPS, TMPD or TMPR 
                 word OpAddrNo,          // address of Operand (OpAddr1 | OpAddr2), for addressMode 3
                 enum dataSize dsz,      // Data Size                             
                 enum addressmode mode,  // required Addressing Mode      
                 byte RegNo            ) // Register number for A[n] or D[n]  
{
  RW = Read ;
  
  switch( mode )
  {
    case DATA_REGISTER_DIRECT:
           *tmpReg = D[RegNo];
           if( dsz == byteSize )
             SetByte( tmpReg, 1, 0 );
           if( dsz <= wordSize )
             SetWord( tmpReg, Most, 0 );
           break;
           
    case ADDRESS_REGISTER_DIRECT: 
           *tmpReg = A[RegNo];
           break;
           
    case RELATIVE_ABSOLUTE:
           // We need to access memory, except for branching & MOVA.
           MAR = OpAddrNo;
           accessMemory( dsz );
           *tmpReg = MDR ;
           break;
           
    case ADDRESS_REGISTER_INDIRECT:
           // We need to access memory.
           MAR = A[RegNo];
           accessMemory( dsz );
           *tmpReg = MDR ;
           break;
           
    case ADDRESS_REGISTER_INDIRECT_POSTINC:
           // We need to access memory. 
           MAR = A[RegNo];
           accessMemory( dsz );
           *tmpReg = MDR ;
           A[RegNo] = A[RegNo] + NOB(dsz);
           break;
           
    case ADDRESS_REGISTER_INDIRECT_PREDEC:
           // We need to access memory. 
           A[RegNo] = A[RegNo] - NOB(dsz);
           MAR = A[RegNo];
           accessMemory( dsz );
           *tmpReg = MDR ;
           break;
           
    default: // This error should never occur, but just in case...! 
             printf( "*** ERROR >> FillTMP() has Invalid Addressing Mode '%d' at PC = %d\n", mode, (PC-2) );
             H = True ;
  }// switch mode
  
}// FillTmpReg()

// Transfer the contents of temporary register to Register OR Memory 
void SetResult( int* tmpReg,            // Source Register (TMPD...)     
                word OpAddrNo,          // Operand Address (OpAddr1...)  
                enum dataSize dsz,      // Data Size                    
                enum addressmode mode,  // required Addressing Mode     
                byte RegNo            ) // Register Number for A[n] or D[n] 
{
  RW = Write ;
  
  // Depends on Addressing Mode 
  switch( mode )
  {
    case DATA_REGISTER_DIRECT: 
           switch( dsz )
           {
             case byteSize: SetBits( &(D[RegNo]), 0, 7, *tmpReg ); break;
             case wordSize: SetWord( &(D[RegNo]), Least, GetWord(*tmpReg, Least) ); break;
             case  intSize: D[RegNo] = *tmpReg; break;
             
             default: printf( "*** ERROR >> SetResult() received invalid data size '%d' at PC = %d\n", dsz, (PC-2) );
                      H = True ;
           }
           break;
           
    case ADDRESS_REGISTER_DIRECT: 
           A[RegNo] = GetWord( *tmpReg, Least );
           break;
           
    case RELATIVE_ABSOLUTE:
           // We need to access memory, except for branching & MOVA.
           MAR = OpAddrNo;
           MDR = *tmpReg;
           accessMemory( dsz );
           break;
           
    case ADDRESS_REGISTER_INDIRECT:
           // We need to access memory.
           MAR = A[RegNo];
           MDR = *tmpReg;
           accessMemory( dsz );
           break;
           
    case ADDRESS_REGISTER_INDIRECT_POSTINC:
           // We need to access memory.                              
           // ATTENTION: for some instructions, the address register has already been incremented by FillTmpReg                
           // DO NOT increment it a 2nd time here
           MAR = A[RegNo] - NOB(dsz);
           MDR = *tmpReg;
           accessMemory( dsz );
           break;
           
    case ADDRESS_REGISTER_INDIRECT_PREDEC:
           // We need to access memory.                              
           // ATTENTION: for some instructions, the address register has already been decremented by FillTmpReg                
           // DO NOT decrement it a 2nd time here
           MAR = A[RegNo];
           MDR = *tmpReg;
           accessMemory( dsz );
           break;
           
    default: // invalid addressMode
            printf( "*** ERROR >> SetResult() has Invalid Addressing Mode '%d' at PC = %d\n", mode, (PC-2) );
            H = True ;
  }// switch mode
  
}// SetResult()

// Status bits Z & N are often set the same way in many instructions
// A function would be useful to do this        
void SetZN( int tmpReg )
{
  switch( DS )
  {
    case byteSize: Z = ( (GetBits(GetWord(tmpReg, Least), 0,  7) | 0) == 0 );
                   N = (  GetBits(GetWord(tmpReg, Least), 7,  7) == 1 );
                   break;
                   
    case wordSize: Z = ( (GetBits(GetWord(tmpReg, Least),  0, 15) | 0) == 0 );
                   N = (  GetBits(GetWord(tmpReg, Least), 15, 15) == 1 );
                   break;
                   
    case  intSize: Z = ( (tmpReg | 0x00000000) == 0 );
                   N = ( GetBits(GetWord(tmpReg, Most), 15, 15) == 1 );
                   break;
                   
    default: printf( "*** ERROR >> SetZN() received invalid data size '%d' at PC = %d\n", DS, (PC-2) );
             H = True ;
  }

}// SetZN()

// The calculations to find V & C are more complex but are simplified by the use of Sm, Dm, Rm
// It would be a good Idea to make a procedure to find these values                                                   
void SetSmDmRm( int tmpSrc, int tmpDst, int tmpRes )
{
  byte mostSigBit = 15 ; // wordSize
  switch( DS )
  {
    case byteSize: mostSigBit =  7 ; break;
    case wordSize: break;
    case  intSize: mostSigBit = 31 ; break;
    
    default: printf( "*** ERROR >> SetSmDmRm() received invalid data size '%d' at PC = %d\n", DS, (PC-2) );
             H = True ;
  }
  
  Sm = ( GetBits(tmpSrc, mostSigBit, mostSigBit) == 1 );
  Dm = ( GetBits(tmpDst, mostSigBit, mostSigBit) == 1 );
  Rm = ( GetBits(tmpRes, mostSigBit, mostSigBit) == 1 );
  
}// SetSmDmRm()

/********************************************************************
  The execution of each instruction is done via its micro-program   
*********************************************************************/
void ExecInstr()
{
  byte i ; // counter 
  word tmpA ;
  
#if DEBUG >= 0
  printf( "\t%s (%s): OpAd1 = %d, OpAd2 = %d, M1 = %d, R1 = %d, M2 = %d, R2 = %d\n",
          Mnemo[OpId], sizeName[DS], OpAddr1, OpAddr1, M1, R1, M2, R2 );
#endif
  
  // Execute the instruction according to opCode
  // Use a CASE structure where each case corresponds to an instruction & its micro-program
  switch( OpId )
  {
     case iADD:
               /* EXAMPLE micro-program according to step 2.4.1 in section 3  */
               // 1. Fill TMPS if necessary 
               FillTmpReg( &TMPS, OpAddr1, DS, M1, R1 );
               // 2. Fill TMPD if necessary 
               FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
               // 3. Compute TMPR using TMPS & TMPD  
               TMPR = TMPS + TMPD ;
#if DEBUG > 0
               printf( "TMPR = %#X(%d), TMPS = %#X(%d), TMPD = %#X(%d)\n", TMPR, TMPR, TMPS, TMPS, TMPD, TMPD );
#endif
               // 4. Update status bits HZNVC if necessary  
               SetZN( TMPR );
               SetSmDmRm( TMPS, TMPD, TMPR );
               V = ( Sm & Dm & ~Rm ) | ( ~Sm & ~Dm & Rm );
               C = ( Sm & Dm ) | ( ~Rm & Dm ) | ( Sm & ~Rm );
               // 5. Store the result in the destination if necessary 
               SetResult( &TMPR, OpAddr2, DS, M2, R2 );
               break ;
     // add quick
     case iADDQ:
                FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
                TMPS = 0 ;
                SetByte( &TMPS, 0, opcData );
                // Sign extension if W or L ??
                TMPR = TMPD + TMPS ;
#if DEBUG > 0
                printf( "TMPR = %#X(%d), TMPS = %#X(%d), TMPD = %#X(%d)\n", TMPR, TMPR, TMPS, TMPS, TMPD, TMPD );
#endif
                SetZN( TMPR );
                SetSmDmRm( TMPS, TMPD, TMPR );
                V = ( Sm & Dm & ~Rm ) | ( ~Sm & ~Dm & Rm );
                C = ( Sm & Dm ) | ( ~Rm & Dm ) | ( Sm & ~Rm );
                SetResult( &TMPR, OpAddr2, DS, M2, R2 );
                break;
                
     case iSUB:
               FillTmpReg( &TMPS, OpAddr1, DS, M1, R1 );
               FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
               TMPR = TMPD - TMPS ;
               SetZN( TMPR );
               SetSmDmRm( TMPS, TMPD, TMPR );
               V = ( ~Sm & Dm & ~Rm ) | ( Sm & ~Dm & Rm );
               C = ( Sm & ~Dm ) | ( Rm & ~Dm ) | ( Sm & Rm );
               SetResult( &TMPR, OpAddr2, DS, M2, R2 );
               break;
     // sub quick
     case iSUBQ:
                FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
                TMPS = 0;
                SetByte( &TMPS, 0, opcData );
                // Sign extension if W or L ??
                TMPR = TMPD - TMPS;
                SetZN( TMPR );
                SetSmDmRm( TMPS, TMPD, TMPR );
                V = ( ~Sm & Dm & ~Rm ) | ( Sm & ~Dm & Rm );
                C = ( Sm & ~Dm ) | ( Rm & ~Dm ) | ( Sm & Rm );
                SetResult( &TMPR, OpAddr2, DS, M2, R2 );
                break;
     // signed
     case iMULS:
                if( CheckCond( (DS == wordSize), "Invalid Data Size" ) )
                {
                  FillTmpReg( &TMPS, OpAddr1, DS, M1, R1 );
                  FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
                  if( GetBits(TMPS,15,15) == 1 )
                    TMPS = TMPS | 0xFFFF0000 ;
                  if( GetBits(TMPD,15,15) == 1 )
                    TMPD = TMPD | 0xFFFF0000 ;
                  TMPR = TMPD * TMPS;
                  SetZN( TMPR );
                  V = False;
                  C = False;
                  SetResult( &TMPR, OpAddr2, intSize, M2, R2 );
                }// if size = 1
                break;
     // signed
     case iDIVS:
                if( CheckCond( (DS == intSize), "Invalid Data Size" ) )
                {
                  FillTmpReg( &TMPS, OpAddr1, wordSize, M1, R1);
                  if( CheckCond( (TMPS != 0), "Division by Zero" ) )
                  {
                    FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
                    V = ( (TMPD / TMPS) < -32768 ) | ( (TMPD / TMPS) > 32767 );
                    if( TMPS > 0x8000 )
                    {
                      i = 1;
                      TMPS = (TMPS ^ 0xFFFF) + 1;
                      TMPD = (TMPD ^ 0xFFFFFFFF) + 1;
                    };
                    if( ((TMPD / TMPS) == 0) && (i == 1) )
                    {
                      SetWord( &TMPR, Least, 0 );
                      TMPD = (TMPD ^ 0xFFFFFFFF) + 1 ;
                      SetWord( &TMPR, Most, TMPD % TMPS );
                    }
                    else
                      {
                        TMPR = TMPD / GetWord(TMPS, Least);
                        SetWord( &TMPR, Most, (TMPD % GetWord(TMPS, Least)) );
                      };
                    SetZN( TMPR );
                    C = False ;
                    SetResult( &TMPR, OpAddr2, DS, M2, R2 );
                  }// if not div by 0
                }// if intSize
                break;
     // negate
     case iNEG:
               FillTmpReg( &TMPD, OpAddr1, DS, M1, R1 );
               TMPR = -TMPD;
               SetZN( TMPR );
               SetSmDmRm( TMPS, TMPD, TMPR );
               V = Dm & Rm ;
               C = Dm | Rm ;
               SetResult( &TMPR, OpAddr1, DS, M1, R1 );
               break;
     // clear
     case iCLR:
               TMPD = 0 ;
               SetZN( TMPD );
               V = False;
               C = False;
               SetResult( &TMPD, OpAddr1, DS, M1, R1 );
               break;
     // logical
     case iNOT:
               FillTmpReg( &TMPD, OpAddr1, DS, M1, R1 );
               TMPR = !TMPD ;
               SetZN( TMPR );
               V = False;
               C = False;
               SetResult( &TMPR, OpAddr1, DS, M1, R1 );
               break;
     // logical
     case iAND:
               FillTmpReg( &TMPS, OpAddr1, DS, M1, R1 );
               FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
               TMPR = TMPD && TMPS ;
               SetZN( TMPR );
               V = False;
               C = False;
               SetResult( &TMPR, OpAddr2, DS, M2, R2 );
               break;
     // logical
     case iOR:
              FillTmpReg( &TMPS, OpAddr1, DS, M1, R1 );
              FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
              TMPR = TMPD || TMPS ;
              SetZN( TMPR );
              V = False;
              C = False;
              SetResult( &TMPR, OpAddr2, DS, M2, R2 );
              break;
     // logical
     case iEOR:
               FillTmpReg( &TMPS, OpAddr1, DS, M1, R1 );
               FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
               TMPR = TMPD ^ TMPS;
               SetZN( TMPR );
               V = False;
               C = False;
               SetResult( &TMPR, OpAddr2, DS, M2, R2 );
               break;
     // logical
     case iLSL:
               FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
               TMPR = TMPD << opcData;
               SetZN( TMPR );
               V = False;
               if( opcData > 0 )
               {
                 C = ( GetBits(TMPD, NOB(DS)*8-opcData, NOB(DS)*8-opcData) == 1 ) ? True : False ;
               }
               else
                   C = False;
               SetResult( &TMPR, OpAddr2, DS, M2, R2 );
               break;
     // logical
     case iLSR:
               FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
               TMPR = TMPD >> opcData ;
               SetZN( TMPR );
               V = False;
               C = ( opcData > 0 ) ? ( GetBits(TMPD, opcData-1, opcData-1) == 1 ) : False ;
               SetResult( &TMPR, OpAddr2, DS, M2, R2 );
               break;
     // rotate
     case iROL:
               FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
               opcData = opcData % (8 * NOB(DS) );
               TMPR = TMPD << opcData;
               TMPS = TMPD >> ( (8*NOB(DS)) - opcData );
               SetBits( &TMPR, 0, opcData-1, TMPS );
               SetZN( TMPR );
               V = False;
               C = ( opcData > 0 ) ? ( GetBits(TMPD, (NOB(DS)*8)-opcData, (NOB(DS)*8)-opcData) == 1 ) : False ;
               SetResult( &TMPR, OpAddr2, DS, M2, R2 );
               break;
     // rotate
     case iROR:
               FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
               opcData = opcData % ( 8*NOB(DS) );
               TMPR = TMPD >> opcData;
               SetBits( &TMPR, (8*NOB(DS))-opcData, (8*NOB(DS))-1, TMPD );
               SetZN( TMPR );
               V = False;
               C = ( opcData > 0 ) ? ( GetBits(TMPD, opcData-1, opcData-1) == 1 ) : False ;
               SetResult( &TMPR, OpAddr2, DS, M2, R2 );
               break;
     // compare
     case iCMP:
               FillTmpReg( &TMPS, OpAddr1, DS, M1, R1 );
               FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
               TMPR = TMPD - TMPS;
               SetZN( TMPR );
               SetSmDmRm (TMPS, TMPD, TMPR );
               V = ( ~Sm & Dm & ~Rm ) | ( Sm & ~Dm & Rm );
               C = ( Sm & ~Dm ) | ( Rm & ~Dm ) | ( Sm & Rm );
               break;
     // test
     case iTST: 
               FillTmpReg( &TMPD, OpAddr1, DS, M1, R1 );
               SetZN( TMPD );
               V = False ;
               C = False ;
               break;
     // branch
     case iBRA:
               if( CheckCond( (M1 == RELATIVE_ABSOLUTE), "Invalid Addressing Mode" ) 
                   && CheckCond( (DS == wordSize), "Invalid Data Size" ) )
                 PC = OpAddr1 ;
               break;
     // branch if overflow
     case iBVS:
               if( CheckCond( (M1 == RELATIVE_ABSOLUTE), "Invalid Addressing Mode" ) 
                   && CheckCond( (DS == wordSize), "Invalid Data Size" ) )
                 if( V == True ) PC = OpAddr1 ;
               break;
     // branch if equal
     case iBEQ:
               if( CheckCond( (M1 == RELATIVE_ABSOLUTE), "Invalid Addressing Mode" ) 
                   && CheckCond( (DS == wordSize), "Invalid Data Size" ) )
                 if( Z == True ) PC = OpAddr1 ;
               break;
     // branch if carry
     case iBCS:
               if( CheckCond( (M1 == RELATIVE_ABSOLUTE), "Invalid Addressing Mode" ) 
                   && CheckCond( (DS == wordSize), "Invalid Data Size" ) )
                 if( C == True ) PC = OpAddr1 ;
               break;
     // branch if GTE
     case iBGE:
               if( CheckCond( (M1 == RELATIVE_ABSOLUTE), "Invalid Addressing Mode" ) 
                   && CheckCond( (DS == wordSize), "Invalid Data Size" ) )
                 if( ~(N^V) == True ) PC = OpAddr1 ;
               break;
     // branch if LTE
     case iBLE:
               if( CheckCond( (M1 == RELATIVE_ABSOLUTE), "Invalid Addressing Mode" ) 
                   && CheckCond( (DS == wordSize), "Invalid Data Size" ) )
                 if( (N^V) == True ) PC = OpAddr1 ;
               break;
               
     case iMOV:
                FillTmpReg( &TMPS, OpAddr1, DS, M1, R1 );
                SetResult( &TMPS, OpAddr2, DS, M2, R2 );
                break;
     // quick
     case iMOVQ:
                 FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
                 SetByte( &TMPD, 0, opcData );
                 // Sign extension if W or L ??
                 SetZN( TMPD );
                 V = False;
                 C = False;
                 SetResult( &TMPD, OpAddr2, DS, M2, R2 );
                 break;
     // exchange
     case iEXG:
               if( CheckCond( ((M1 <= ADDRESS_REGISTER_DIRECT) && (M2 <= ADDRESS_REGISTER_DIRECT)), "Invalid Addressing Mode" ) )
               {
                 FillTmpReg( &TMPS, OpAddr1, DS, M1, R1 );
                 FillTmpReg( &TMPD, OpAddr2, DS, M2, R2 );
                 SetResult(  &TMPS, OpAddr1, DS, M2, R2 );
                 SetResult(  &TMPD, OpAddr2, DS, M1, R1 );
                 V = False;
                 C = False;
               }
               break;
     // address
     case iMOVA:
                 if( CheckCond( ((M1 == RELATIVE_ABSOLUTE) && (M2 == ADDRESS_REGISTER_DIRECT)), "Invalid Addressing Mode" )
                     && CheckCond( (DS == wordSize), "Invalid Data Size" ) )
                   SetResult( &OpAddr1, OpAddr2, DS, M2, R2 );
                 break;
     // input
     case iINP:
               printf( "Enter a value " );
               switch( DS )
               {
                 case byteSize: printf( "(%s) for ", sizeName[byteSize] ); break;
                 case wordSize: printf( "(%s) for ", sizeName[wordSize] ); break;
                 case  intSize: printf( "(%s) for ", sizeName[intSize] ); break;
                 
                 default: printf( "\n*** ERROR >> ExecInstr() invalid data size '%d' for instruction '%d' at PC = %d\n",
                                  DS, OpId, (PC-2) );
                          H = True ;
                          return ;
               }
               switch( M1 )
               {
                 case DATA_REGISTER_DIRECT:    printf( "the register D%d", R1 ); break;
                 case ADDRESS_REGISTER_DIRECT: printf( "the register A%d", R1 ); break;
                 
                 case ADDRESS_REGISTER_INDIRECT: 
                 case ADDRESS_REGISTER_INDIRECT_PREDEC: 
                 case ADDRESS_REGISTER_INDIRECT_POSTINC: printf( "the memory address %#X", A[R1]/*:4*/ ); break;
                 
                 case RELATIVE_ABSOLUTE: printf( "the memory address %#X", OpAddr1 ); break;
                 
                 default: printf( "\n*** ERROR >> ExecInstr() invalid mode type '%d' for instruction '%d' at PC = %d\n",
                                  M1, OpId, (PC-2) );
                          H = True ;
                          return ;
               }
               printf( ": " );
               scanf( "%x", &TMPD );
               SetZN( TMPD );
               C = False;
               V = False;
               SetResult( &TMPD, OpAddr1, DS, M1, R1 );
               break;
     // display
     case iDSP:
               FillTmpReg( &TMPS, OpAddr1, DS, M1, R1 );
               switch( M1 )
               {
                 case DATA_REGISTER_DIRECT:      printf( "[ D%d ] = ", R1 ); break;
                 case ADDRESS_REGISTER_DIRECT:   printf( "[ A%d ] = ", R1 ); break;
                 case ADDRESS_REGISTER_INDIRECT: printf( "[%#X] = ", A[R1] ); break;
                 
                 case ADDRESS_REGISTER_INDIRECT_POSTINC: // NOB(DS) subtracted to compensate post-incrementation 
                         tmpA = A[R1] - NOB(DS);
                         printf( "[%#X] = ", tmpA );
                         break;
                         
                 case ADDRESS_REGISTER_INDIRECT_PREDEC: printf( "[%#X] = ", A[R1] ); break;
                 case RELATIVE_ABSOLUTE: printf( "[%#X] = ", OpAddr1/*:4*/ ); break;
                 
                 default: printf( "\n*** ERROR >> ExecInstr() invalid mode type '%d' for instruction '%d' at PC = %d\n",
                                  M1, OpId, (PC-2) );
                          H = True ;
                          return ;
               }
               switch( DS )
               {
                 case byteSize: printf( "%#X (%s)\n", TMPS, sizeName[byteSize] ); break;
                 case wordSize: printf( "%#X (%s)\n", TMPS, sizeName[wordSize] ); break;
                 case  intSize: printf( "%#X (%s) \n", TMPS, sizeName[intSize] ); break;
                 
                 default: printf( "\n*** ERROR >> ExecInstr() invalid data size '%d' for instruction '%d' at PC = %d\n",
                                  DS, OpId, (PC-2) );
                          H = True ;
                          return ;
               }
               break;
     // display status register
     case iDSR: printf( "Status Bits: H:%s N:%s Z:%s V:%s C:%s \n",
                        bitName[(int)H], bitName[(int)N], bitName[(int)Z], bitName[(int)V], bitName[(int)C] );
                break;
     // halt
     case iHLT: H = True; // Set the Halt bit to True (stops program)
                break;
                
     default: printf( "*** ERROR >> ExecInstr() received invalid instruction '%d' at PC = %d\n", OpId, (PC-2) );
              H = True ;
              
  }// switch( OpId )
  
}// ExecInstr()

// Initialize the simulator (PC & status bits) 
void Init()
{
  PC = 0 ;
  C = False;
  V = False;
  Z = False;
  N = False;
  H = False;
  
}// Init()

// Fetch-Execute Cycle simulated
void Controller()
{
  Init();

  do // Repeat the Fetch-Execute Cycle until the Halt bit becomes True
  {
    FetchOpCode();
    DecodeInstr();
    FetchOperands();
    if( H == False )
      ExecInstr();
  }
  while( H == False );
  
  puts( "\n\tEnd of Fetch-Execute Cycle" );
  
}// Controller()

/*
 *  MAIN
 **************************************************************************/
int main( int argc, char* argv[] )
{
  char option ; // option chosen from the menu by the user
  char program_name[ memorySize ];
  
  // info on system data sizes
#if DEBUG > 1
  printf( "sizeof( char ) == %d\n", sizeof(char) );
  printf( "sizeof( short ) == %d\n", sizeof(short) );
  printf( "sizeof( int ) == %d\n", sizeof(int) );
  printf( "sizeof( long ) == %d\n", sizeof(long) );
  printf( "sizeof( bit ) == %d\n", sizeof(enum bit) );
  printf( "sizeof( rwbit ) == %d\n", sizeof(enum rwbit) );
  printf( "sizeof( dataSize ) == %d\n", sizeof(enum dataSize) );
  printf( "sizeof( numOperands ) == %d\n", sizeof(enum numOperands) );
  printf( "sizeof( boolean ) == %d\n", sizeof(boolean) );
  printf( "sizeof( string ) == %d\n", sizeof(string) );
  printf( "sizeof( byte ) == %d\n", sizeof(byte) );
  printf( "sizeof( word ) == %d\n", sizeof(word) );
#endif

  MnemoInit();
  
  // Menu 
  while( option != QUIT )
  {
    if( ! isspace(option) )
      printf( "Your Option ('%c' to execute, '%c' to quit): ", EXECUTE, QUIT );
    
    scanf( "%c", &option );
    switch( option )
    {
      case EXECUTE :
                    // Execution on the simulator
                    printf( "Name of the 68k binary program ('.68b' will be added automatically): " );
                    scanf( "%s", program_name );
                    if( Loader(strcat(program_name, ".68b")) )
                      Controller(); // Start the simulator
                    
                    break;
      
      case QUIT : puts( "Bye!" );
                  break;
                  
      case EOL : break;
      
      default: printf( "Invalid Option. Please enter '%c' or '%c' \n", EXECUTE, QUIT );
    }
    
  }// while
  
  puts( "\tPROGRAM ENDED" );
  return 0 ;

}// main()
